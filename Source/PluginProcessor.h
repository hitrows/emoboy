#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "Presets.h"
#include "dsp/PitchFormantEngine.h"
#include "dsp/PitchDetector.h"
#include "dsp/Drive.h"
#include "dsp/AutoGain.h"
#if EMOBOY_NERD_FEATURES
#include "dsp/Modulation.h"
#endif

class EmoBoyProcessor : public juce::AudioProcessor
{
public:
    EmoBoyProcessor();
    ~EmoBoyProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Standard host-recognised programs (2026-08-21) - the 9 entries in
    // Presets.h, so Logic's own preset selector can reach all of them, not
    // just the first 4 that also have a panel footswitch. setCurrentProgram
    // applies the preset's parameter values and is the single path both a
    // host preset-menu pick and a top-row button click go through (see
    // EmoBoyEditor::applyPreset()) - hasExplicitProgram() tells the editor
    // whether a program has actually been selected at least once, so a
    // fresh instance (params still at their own transparent defaults,
    // matching none of the 9 presets) doesn't show "Cry" highlighted just
    // because index 0 is JUCE's own default getCurrentProgram().
    int getNumPrograms() override { return (int) Presets::table.size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override { return Presets::table[(size_t) index].name; }
    void changeProgramName (int, const juce::String&) override {}
    bool hasExplicitProgram() const noexcept { return programWasExplicitlySet; }

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter (Param::bypass); }

    // The PEAK lamp's state, written on the audio thread every block,
    // read on the message thread at 30Hz by the editor - hence atomic.
    // Not a smoothed level meter: a fast/instant on the moment the input
    // crosses a threshold, held for a short time, then a hard snap off -
    // "jumps between 0 and 100", the user's own words, not a fade.
    bool isPeakLedOn() const noexcept { return peakLedOn.load (std::memory_order_relaxed); }

    // User-savable preset slots (WRITE + the 4 icon buttons, 2026-08-21).
    // UI-thread-only (message thread: button clicks, plus
    // getStateInformation/setStateInformation, which hosts also call from
    // the message thread in practice) - no audio-thread access, so no
    // atomics needed here unlike the PEAK lamp above. Persisted as part of
    // the plugin's own state (see getStateInformation) so they survive a
    // project reload, like a real WRITE button's memory would.
    struct UserPresetSlot
    {
        bool hasData = false;
        float pitch = 0.0f, formant = 0.0f, drive = 0.0f, mix = 100.0f;
        int mode = 0, robotNoteIndex = 12;
    };
    std::array<UserPresetSlot, 4> userPresets;

    juce::AudioProcessorValueTreeState apvts;

private:
    // Cached raw pointers into the APVTS - avoids a string lookup per
    // parameter per block. All of these are owned by apvts; never delete.
    std::atomic<float>* pPitch = nullptr;
    std::atomic<float>* pFormant = nullptr;
    std::atomic<float>* pLink = nullptr;
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pRobotNote = nullptr;
    std::atomic<float>* pDrive = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pBypass = nullptr;

#if EMOBOY_NERD_FEATURES
    std::atomic<float>* pMod1Rate = nullptr;
    std::atomic<float>* pMod1Phase = nullptr;
    std::atomic<float>* pMod1Level = nullptr;
    std::atomic<float>* pMod2Rate = nullptr;
    std::atomic<float>* pMod2Phase = nullptr;
    std::atomic<float>* pMod2Level = nullptr;

    std::atomic<float>* pAmAttack = nullptr;
    std::atomic<float>* pAmRelease = nullptr;
    std::atomic<float>* pAmLevel = nullptr;

    std::atomic<float>* pPtSmooth = nullptr;
    std::atomic<float>* pPtOffset = nullptr;
    std::atomic<float>* pPtLevel = nullptr;

    std::array<std::array<std::atomic<float>*, ModMatrix::numTargets>, ModMatrix::numSources> pRouteOn {};
    std::array<std::array<std::atomic<float>*, ModMatrix::numTargets>, ModMatrix::numSources> pRouteDepth {};
#endif

    PitchFormantEngine engine;
    PitchDetector pitchDetector;
#if EMOBOY_NERD_FEATURES
    LFO lfo1, lfo2;
    EnvelopeFollower envFollower;
    ModMatrix modMatrix;
#endif
    std::vector<Drive> driveStages; // one per channel
    AutoGain autoGain;

    // Dry path delay compensation, matched sample-for-sample to
    // engine.getLatencySamples() so Mix < 100% doesn't comb-filter.
    juce::AudioBuffer<float> dryDelayBuffer;
    int dryDelayWritePos = 0;
    int dryDelaySamples = 0;

#if EMOBOY_NERD_FEATURES
    // PT modulation source smoothing state (one-pole, time constant from
    // the PT Smooth parameter).
    float ptSmoothedSemitones = 0.0f;
#endif

    // Currently-held MIDI note for Robot mode, -1 if none held.
    int heldMidiNote = -1;

    // PEAK lamp: input-level peak-hold detector, audio-thread-only state
    // plus the atomic the UI actually reads.
    std::atomic<bool> peakLedOn { false };
    int peakHoldSamplesRemaining = 0;
    int peakHoldSamples = 0; // set from sample rate in prepareToPlay

    double currentSampleRate = 44100.0;

    int currentProgram = 0;
    bool programWasExplicitlySet = false;

    float currentPitchSemitones = 0.0f;   // for metering/UI if ever needed
    float currentFormantSemitones = 0.0f;

    // Downmixes to mono and feeds the pitch detector - needed unconditionally
    // for Quantize/Robot mode, not just for the (optional) PT modulation
    // source, so this stays outside the EMOBOY_NERD_FEATURES gate.
    void feedPitchDetector (const juce::AudioBuffer<float>& input, int numSamples);
#if EMOBOY_NERD_FEATURES
    void updateModulationSources (const juce::AudioBuffer<float>& input, int numSamples);
#endif
    void handleMidi (const juce::MidiBuffer& midi);
    float midiNoteToHz (int note) const { return 440.0f * std::pow (2.0f, ((float) note - 69.0f) / 12.0f); }
    float hzToSemitone (float hz) const { return hz > 1.0f ? 12.0f * std::log2 (hz / 440.0f) + 69.0f : 0.0f; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmoBoyProcessor)
};
