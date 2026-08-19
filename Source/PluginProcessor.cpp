#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Modulation depth = 100% maps to this much swing on each target.
    // Chosen so full-depth modulation is dramatic but not degenerate -
    // measured by ear against the demo brief's expectation that Mod1 on
    // Pitch/Formant should be clearly audible, not adjusted against any
    // other reference.
    constexpr float kPitchModRangeSemitones = 12.0f;
    constexpr float kFormantModRangeSemitones = 12.0f;
    constexpr float kMixModRangePercent = 50.0f;
    constexpr float kDriveModRangePercent = 50.0f;

    constexpr int kRobotNoteBase = 36; // index 0 ("C2" in the selector) = MIDI note 36
}

EmoBoyProcessor::EmoBoyProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    pPitch = apvts.getRawParameterValue (Param::pitch);
    pFormant = apvts.getRawParameterValue (Param::formant);
    pLink = apvts.getRawParameterValue (Param::link);
    pMode = apvts.getRawParameterValue (Param::mode);
    pRobotNote = apvts.getRawParameterValue (Param::robotNote);
    pDrive = apvts.getRawParameterValue (Param::drive);
    pMix = apvts.getRawParameterValue (Param::mix);

    pMod1Rate = apvts.getRawParameterValue (Param::mod1Rate);
    pMod1Phase = apvts.getRawParameterValue (Param::mod1Phase);
    pMod1Level = apvts.getRawParameterValue (Param::mod1Level);
    pMod2Rate = apvts.getRawParameterValue (Param::mod2Rate);
    pMod2Phase = apvts.getRawParameterValue (Param::mod2Phase);
    pMod2Level = apvts.getRawParameterValue (Param::mod2Level);

    pAmAttack = apvts.getRawParameterValue (Param::amAttack);
    pAmRelease = apvts.getRawParameterValue (Param::amRelease);
    pAmLevel = apvts.getRawParameterValue (Param::amLevel);

    pPtSmooth = apvts.getRawParameterValue (Param::ptSmooth);
    pPtOffset = apvts.getRawParameterValue (Param::ptOffset);
    pPtLevel = apvts.getRawParameterValue (Param::ptLevel);

    for (int s = 0; s < ModMatrix::numSources; ++s)
    {
        for (int t = 0; t < ModMatrix::numTargets; ++t)
        {
            auto source = (Param::Source) s;
            auto target = (Param::Target) t;
            pRouteOn[(size_t) s][(size_t) t] = apvts.getRawParameterValue (Param::routeOnId (source, target));
            pRouteDepth[(size_t) s][(size_t) t] = apvts.getRawParameterValue (Param::routeDepthId (source, target));
        }
    }
}

void EmoBoyProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    const int numChannels = juce::jmax (1, getTotalNumInputChannels());

    engine.prepare (sampleRate, numChannels);
    setLatencySamples (engine.getLatencySamples());

    pitchDetector.prepare (sampleRate, 2048);

    lfo1.prepare (sampleRate);
    lfo2.prepare (sampleRate);
    envFollower.prepare (sampleRate);

    driveStages.assign ((size_t) numChannels, Drive {});
    for (auto& d : driveStages)
        d.prepare (sampleRate);

    dryDelaySamples = engine.getLatencySamples();
    dryDelayBuffer.setSize (numChannels, juce::jmax (1, dryDelaySamples));
    dryDelayBuffer.clear();
    dryDelayWritePos = 0;

    ptSmoothedSemitones = 0.0f;
    heldMidiNote = -1;
}

bool EmoBoyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == mono || in == stereo;
}

void EmoBoyProcessor::handleMidi (const juce::MidiBuffer& midi)
{
    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            heldMidiNote = msg.getNoteNumber();
        else if (msg.isNoteOff() && msg.getNoteNumber() == heldMidiNote)
            heldMidiNote = -1;
    }
}

void EmoBoyProcessor::updateModulationSources (const juce::AudioBuffer<float>& input, int numSamples)
{
    const float lfo1Value = lfo1.process (numSamples, pMod1Rate->load());
    const float lfo2Value = lfo2.process (numSamples, pMod2Rate->load());

    // Phase offsets are applied as a constant rotation of the running
    // phase rather than per-block reset, so re-reading them every block is
    // cheap and correct - see LFO::setPhaseOffsetDegrees.
    lfo1.setPhaseOffsetDegrees (pMod1Phase->load());
    lfo2.setPhaseOffsetDegrees (pMod2Phase->load());

    juce::AudioBuffer<float> mono (1, numSamples);
    mono.clear();
    for (int c = 0; c < input.getNumChannels(); ++c)
        mono.addFrom (0, 0, input, c, 0, numSamples, 1.0f / (float) input.getNumChannels());

    const float envValue = envFollower.process (mono.getReadPointer (0), numSamples,
                                                  pAmAttack->load(), pAmRelease->load());

    pitchDetector.pushBlock (mono.getReadPointer (0), numSamples);

    float ptValue = 0.0f;
    if (pitchDetector.isVoiced())
    {
        const float detectedSemitone = hzToSemitone (pitchDetector.getFrequencyHz());
        const float raw = detectedSemitone - (69.0f + pPtOffset->load());
        const float smoothAmount = juce::jlimit (0.0f, 0.995f, pPtSmooth->load() / 100.0f);
        ptSmoothedSemitones += (1.0f - smoothAmount) * (raw - ptSmoothedSemitones);
    }
    ptValue = juce::jlimit (-1.0f, 1.0f, ptSmoothedSemitones / 24.0f);

    modMatrix.sourceValue[0] = lfo1Value;
    modMatrix.sourceValue[1] = lfo2Value;
    modMatrix.sourceValue[2] = envValue; // unipolar 0..1 by design - see Modulation.h
    modMatrix.sourceValue[3] = ptValue;

    for (int s = 0; s < ModMatrix::numSources; ++s)
    {
        for (int t = 0; t < ModMatrix::numTargets; ++t)
        {
            modMatrix.enabled[(size_t) s][(size_t) t] = pRouteOn[(size_t) s][(size_t) t]->load() > 0.5f;
            modMatrix.depth[(size_t) s][(size_t) t] = pRouteDepth[(size_t) s][(size_t) t]->load() / 100.0f;
        }
    }

    // Scale each source's individual level knob directly into its stored
    // value so Level=0 mutes that source's contribution everywhere it is
    // routed, without touching per-target depths.
    modMatrix.sourceValue[0] *= pMod1Level->load() / 100.0f;
    modMatrix.sourceValue[1] *= pMod2Level->load() / 100.0f;
    modMatrix.sourceValue[2] *= pAmLevel->load() / 100.0f;
    modMatrix.sourceValue[3] *= pPtLevel->load() / 100.0f;
}

void EmoBoyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    handleMidi (midi);
    updateModulationSources (buffer, numSamples);

    // ---- resolve Pitch / Formant target for this block -----------------
    const auto mode = (Param::Mode) (int) pMode->load();
    const float pitchKnob = pPitch->load();
    const float formantKnob = pFormant->load();
    const bool link = pLink->load() > 0.5f;

    float totalPitchSemitones = pitchKnob;

    if (mode == Param::Mode::Quantize)
    {
        if (pitchDetector.isVoiced())
        {
            const float detected = hzToSemitone (pitchDetector.getFrequencyHz());
            const float nearest = std::round (detected);
            totalPitchSemitones = (nearest - detected) + pitchKnob;
        }
    }
    else if (mode == Param::Mode::Robot)
    {
        const float targetSemitone = heldMidiNote >= 0
            ? (float) heldMidiNote
            : hzToSemitone (midiNoteToHz (kRobotNoteBase + (int) pRobotNote->load()));

        if (pitchDetector.isVoiced())
        {
            const float detected = hzToSemitone (pitchDetector.getFrequencyHz());
            totalPitchSemitones = targetSemitone - detected;
        }
        // else: hold the last correction - do not snap to 0 on silence,
        // which would produce an audible pitch jump every time the singer
        // takes a breath.
        else
        {
            totalPitchSemitones = currentPitchSemitones;
        }
    }

    float effectiveFormantSemitones = formantKnob + (link ? pitchKnob : 0.0f);

    totalPitchSemitones += modMatrix.sumFor ((int) Param::Target::Pitch) * kPitchModRangeSemitones;
    effectiveFormantSemitones += modMatrix.sumFor ((int) Param::Target::Formant) * kFormantModRangeSemitones;

    totalPitchSemitones = juce::jlimit (-36.0f, 36.0f, totalPitchSemitones);
    effectiveFormantSemitones = juce::jlimit (-36.0f, 36.0f, effectiveFormantSemitones);

    currentPitchSemitones = totalPitchSemitones;
    currentFormantSemitones = effectiveFormantSemitones;

    const float pitchRatio = std::pow (2.0f, totalPitchSemitones / 12.0f);
    const float formantRatio = std::pow (2.0f, effectiveFormantSemitones / 12.0f);

    float mixPercent = pMix->load() + modMatrix.sumFor ((int) Param::Target::Mix) * kMixModRangePercent;
    mixPercent = juce::jlimit (0.0f, 100.0f, mixPercent);

    float drivePercent = pDrive->load() + modMatrix.sumFor ((int) Param::Target::Drive) * kDriveModRangePercent;
    drivePercent = juce::jlimit (0.0f, 100.0f, drivePercent);
    const float driveAmount = drivePercent / 100.0f;

    // ---- dry path: delay to match the engine's analysis latency --------
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf (buffer);

    if (dryDelaySamples > 0)
    {
        for (int c = 0; c < numChannels; ++c)
        {
            auto* dry = dryBuffer.getWritePointer (c);
            auto* delayLine = dryDelayBuffer.getWritePointer (juce::jmin (c, dryDelayBuffer.getNumChannels() - 1));
            int w = dryDelayWritePos;
            for (int i = 0; i < numSamples; ++i)
            {
                const float delayed = delayLine[w];
                delayLine[w] = dry[i];
                dry[i] = delayed;
                w = (w + 1) % dryDelaySamples;
            }
        }
        dryDelayWritePos = (dryDelayWritePos + numSamples) % dryDelaySamples;
    }

    // ---- wet path: pitch/formant engine, then Drive ---------------------
    engine.process (buffer, pitchRatio, formantRatio);

    for (int c = 0; c < numChannels; ++c)
    {
        auto& drv = driveStages[(size_t) juce::jmin (c, (int) driveStages.size() - 1)];
        auto* wet = buffer.getWritePointer (c);
        for (int i = 0; i < numSamples; ++i)
            wet[i] = drv.processSample (wet[i], driveAmount);
    }

    // ---- mix --------------------------------------------------------------
    const float wetGain = mixPercent / 100.0f;
    const float dryGain = 1.0f - wetGain;
    for (int c = 0; c < numChannels; ++c)
    {
        auto* wet = buffer.getWritePointer (c);
        auto* dry = dryBuffer.getReadPointer (juce::jmin (c, dryBuffer.getNumChannels() - 1));
        for (int i = 0; i < numSamples; ++i)
            wet[i] = dry[i] * dryGain + wet[i] * wetGain;
    }
}

juce::AudioProcessorEditor* EmoBoyProcessor::createEditor()
{
    return new EmoBoyEditor (*this);
}

void EmoBoyProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
    }
}

void EmoBoyProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EmoBoyProcessor();
}
