cmake_minimum_required(VERSION 3.22)

# ---------------------------------------------------------------------------
# Identity. Working name is "EmoBoy" per the user - NOT final branding, but
# it is what they asked for, so it is used directly (the demo brief's
# suggested placeholder name/bundle id is not used).
# ---------------------------------------------------------------------------
set(PROJECT_NAME        "EmoBoy")
set(PRODUCT_NAME        "EmoBoy")
set(COMPANY_NAME        "Hitrows")
set(BUNDLE_ID           "com.hitrows.emoboy")
set(MANUFACTURER_CODE   Htrw)
set(PLUGIN_CODE         Emob)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures")

project(${PROJECT_NAME} VERSION 0.1.7)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Demo scope: AU + Standalone only, Apple Silicon only, this machine only.
set(EMOBOY_FORMATS AU Standalone)

# ---------------------------------------------------------------------------
# Mod1/Mod2/AM/PT + the 4x4 modulation routing matrix - fully implemented,
# parked at the user's request (2026-08-19, see HANDOFF.md) for a future
# "EmoBoy Nerd" variant. OFF means it is not merely hidden in the UI but
# genuinely not compiled into this build's parameters, DSP, or editor - see
# the EMOBOY_NERD_FEATURES guards in Source/Parameters.*,
# Source/PluginProcessor.*, and Source/PluginEditor.*. Flip to ON to bring
# it all back.
# ---------------------------------------------------------------------------
option(EMOBOY_NERD_FEATURES "Build Mod1/Mod2/AM/PT modulation + routing matrix" OFF)

# ---------------------------------------------------------------------------
# JUCE. Same pin as the "Not Sure" project (8.0.15) for consistency. Reuse
# the already-cloned checkout from that project's build dir if present, to
# avoid a second multi-hundred-MB clone.
# ---------------------------------------------------------------------------
include(FetchContent)

if (EXISTS "/Users/hitrows/Developer/Not Sure/build/_deps/juce-src/CMakeLists.txt")
    set(FETCHCONTENT_SOURCE_DIR_JUCE "/Users/hitrows/Developer/Not Sure/build/_deps/juce-src")
endif()

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)

FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# Plugin target
# ---------------------------------------------------------------------------
juce_add_plugin(${PROJECT_NAME}
    COMPANY_NAME                "${COMPANY_NAME}"
    BUNDLE_ID                   "${BUNDLE_ID}"
    PLUGIN_MANUFACTURER_CODE    ${MANUFACTURER_CODE}
    PLUGIN_CODE                 ${PLUGIN_CODE}
    PRODUCT_NAME                "${PRODUCT_NAME}"

    IS_SYNTH                    FALSE
    # MIDI input was tried for the Robot-mode "follow a held MIDI note"
    # bonus feature, but NEEDS_MIDI_INPUT makes JUCE register the AU as a
    # Music Effect ("aumf") instead of a plain effect ("aufx") - Logic then
    # needs a MIDI-capable insert slot instead of a normal audio-track
    # insert, which fights the demo's actual goal (drop it on a vocal
    # track, listen). Selector-based Robot note target still works fully;
    # live MIDI is out of scope for this demo build - see HANDOFF.md.
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    COPY_PLUGIN_AFTER_BUILD     TRUE

    FORMATS                     ${EMOBOY_FORMATS})

target_sources(${PROJECT_NAME} PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/dsp/PitchFormantEngine.cpp)

target_compile_definitions(${PROJECT_NAME} PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    EMOBOY_NERD_FEATURES=$<BOOL:${EMOBOY_NERD_FEATURES}>)

# ---------------------------------------------------------------------------
# Panel artwork - the user's hardware-pedal mockup, now split into a clean
# background layer (no text labels, no cap graphics) and a reusable fader
# cap sprite, both supplied by the user (pics/bg-clean.png, pics/faders.png,
# copied into Resources/). Real layered design (per-fader-state art, etc.)
# still comes later; this is "real fader caps that move", not a line.
# ---------------------------------------------------------------------------
juce_add_binary_data(EmoBoyAssets
    SOURCES
        Resources/pedalbg.png
        Resources/fadercap.png
        Resources/robotglow.png
        Resources/bypassglow.png)

set_target_properties(EmoBoyAssets PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        EmoBoyAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# emoboy-render
#
# Offline numeric test harness for PitchFormantEngine - no plugin wrapper,
# no GUI, so it builds fast. Reach for this before opening Logic; see
# tools/render.cpp.
# ---------------------------------------------------------------------------
add_executable (emoboy-render
    tools/render.cpp
    Source/dsp/PitchFormantEngine.cpp)

target_link_libraries (emoboy-render PRIVATE
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

set_target_properties (emoboy-render PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}/tools"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/tools"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/tools"
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)

# ---------------------------------------------------------------------------
# emoboy-preview
#
# Offscreen EmoBoyEditor -> PNG snapshot tool (tools/preview.cpp). Not part
# of the plugin - lets the fader/indicator geometry against the pedal photo
# be checked without screen-recording permission or opening a DAW.
# ---------------------------------------------------------------------------
add_executable (emoboy-preview
    tools/preview.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/dsp/PitchFormantEngine.cpp)

target_compile_definitions (emoboy-preview PRIVATE
    JucePlugin_Name="EmoBoyPreview"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    EMOBOY_NERD_FEATURES=$<BOOL:${EMOBOY_NERD_FEATURES}>)

target_link_libraries (emoboy-preview PRIVATE
    EmoBoyAssets
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

set_target_properties (emoboy-preview PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}/tools"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/tools"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/tools"
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)
