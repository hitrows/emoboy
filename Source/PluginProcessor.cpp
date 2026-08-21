#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Licence.h"

namespace
{
#if EMOBOY_NERD_FEATURES
    // Modulation depth = 100% maps to this much swing on each target.
    // Chosen so full-depth modulation is dramatic but not degenerate -
    // measured by ear against the demo brief's expectation that Mod1 on
    // Pitch/Formant should be clearly audible, not adjusted against any
    // other reference.
    constexpr float kPitchModRangeSemitones = 12.0f;
    constexpr float kFormantModRangeSemitones = 12.0f;
    constexpr float kMixModRangePercent = 50.0f;
    constexpr float kDriveModRangePercent = 50.0f;
#endif

    // index 0 ("C2" in the selector) = MIDI note 48. 2026-08-20: this is
    // Logic Pro's own note numbering (middle C = C3 = MIDI 60), not the
    // ASA/scientific-pitch-notation convention (middle C = C4) the first
    // build used (which put "C2" at MIDI 36, a full octave low against
    // what Logic itself calls C2) - the user asked for the *actual pitch*
    // to match Logic's C2-B3, not just the printed label, so this constant
    // moved, not only the display strings in Parameters.cpp. Index 12 -
    // the knob's 12-o'clock rest position - now lands exactly on Logic's
    // C3 = MIDI 60 = middle C, which is a clean, sensible reference point.
    constexpr int kRobotNoteBase = 48;

    // PEAK lamp threshold and hold time - a first guess (2026-08-20, not
    // audited by ear), meant to react to normal singing level, not just
    // the loudest transients. -18 dBFS linear; easy to retune here.
    constexpr float kPeakThreshold = 0.126f;
    constexpr float kPeakHoldMs = 120.0f;
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
    pBypass = apvts.getRawParameterValue (Param::bypass);

#if EMOBOY_NERD_FEATURES
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
#endif

    // Reads licence.txt once (2026-08-21, ported from "Not Sure"): a local
    // file read, not network, so - unlike UpdateChecker - there is no
    // reason to defer this past the processor constructor. By the time an
    // editor opens the answer is already cached.
    emoboy::LicenceChecker::getInstance();
}

void EmoBoyProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    const int numChannels = juce::jmax (1, getTotalNumInputChannels());

    engine.prepare (sampleRate, numChannels);
    setLatencySamples (engine.getLatencySamples());

    pitchDetector.prepare (sampleRate, 2048);

#if EMOBOY_NERD_FEATURES
    lfo1.prepare (sampleRate);
    lfo2.prepare (sampleRate);
    envFollower.prepare (sampleRate);
#endif

    driveStages.assign ((size_t) numChannels, Drive {});
    for (auto& d : driveStages)
        d.prepare (sampleRate);

    autoGain.prepare (sampleRate);

    dryDelaySamples = engine.getLatencySamples();
    dryDelayBuffer.setSize (numChannels, juce::jmax (1, dryDelaySamples));
    dryDelayBuffer.clear();
    dryDelayWritePos = 0;

#if EMOBOY_NERD_FEATURES
    ptSmoothedSemitones = 0.0f;
#endif
    heldMidiNote = -1;

    peakHoldSamples = (int) (kPeakHoldMs * 0.001 * sampleRate);
    peakHoldSamplesRemaining = 0;
    peakLedOn.store (false, std::memory_order_relaxed);
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

void EmoBoyProcessor::feedPitchDetector (const juce::AudioBuffer<float>& input, int numSamples)
{
    juce::AudioBuffer<float> mono (1, numSamples);
    mono.clear();
    for (int c = 0; c < input.getNumChannels(); ++c)
        mono.addFrom (0, 0, input, c, 0, numSamples, 1.0f / (float) input.getNumChannels());
    pitchDetector.pushBlock (mono.getReadPointer (0), numSamples);
}

#if EMOBOY_NERD_FEATURES
void EmoBoyProcessor::updateModulationSources (const juce::AudioBuffer<float>& input, int numSamples)
{
    const float lfo1Value = lfo1.process (numSamples, pMod1Rate->load());
    const float lfo2Value = lfo2.process (numSamples, pMod2Rate->load());

    // Phase offsets are applied as a constant rotation of the running
    // phase rather than per-block reset, so re-reading them every block is
    // cheap and correct - see LFO::setPhaseOffsetDegrees.
    lfo1.setPhaseOffsetDegrees (pMod1Phase->load());
    lfo2.setPhaseOffsetDegrees (pMod2Phase->load());

    // Pitch detector itself is already fed by feedPitchDetector() (called
    // unconditionally in processBlock, ahead of this) - only the mono
    // downmix for the envelope follower is recomputed here.
    juce::AudioBuffer<float> mono (1, numSamples);
    mono.clear();
    for (int c = 0; c < input.getNumChannels(); ++c)
        mono.addFrom (0, 0, input, c, 0, numSamples, 1.0f / (float) input.getNumChannels());

    const float envValue = envFollower.process (mono.getReadPointer (0), numSamples,
                                                  pAmAttack->load(), pAmRelease->load());

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
#endif // EMOBOY_NERD_FEATURES

void EmoBoyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // PEAK lamp: measured on the raw input, before bypass, so it still
    // works as a signal-present monitor even with BYPASS engaged.
    {
        float peak = 0.0f;
        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            const auto* data = buffer.getReadPointer (c);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                peak = juce::jmax (peak, std::abs (data[i]));
        }

        if (peak >= kPeakThreshold)
            peakHoldSamplesRemaining = peakHoldSamples;
        else
            peakHoldSamplesRemaining = juce::jmax (0, peakHoldSamplesRemaining - buffer.getNumSamples());

        peakLedOn.store (peakHoldSamplesRemaining > 0, std::memory_order_relaxed);
    }

    // Hard bypass: the BYPASS footswitch, wired straight through - leaves
    // the buffer completely untouched, no engine/Drive/auto-gain work at
    // all. Simple and matches "плагин ничего не делает" literally; does
    // NOT attempt latency-compensated "smart" bypass, so toggling it
    // mid-playback can shift timing by the engine's ~46ms latency versus
    // what the host already compensated for - acceptable for this demo,
    // worth revisiting if bypass automation ever needs to be click-free.
    if (pBypass->load() > 0.5f)
        return;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    handleMidi (midi);
    feedPitchDetector (buffer, numSamples);
#if EMOBOY_NERD_FEATURES
    updateModulationSources (buffer, numSamples);
#endif

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

#if EMOBOY_NERD_FEATURES
    totalPitchSemitones += modMatrix.sumFor ((int) Param::Target::Pitch) * kPitchModRangeSemitones;
    effectiveFormantSemitones += modMatrix.sumFor ((int) Param::Target::Formant) * kFormantModRangeSemitones;
#endif

    totalPitchSemitones = juce::jlimit (-36.0f, 36.0f, totalPitchSemitones);
    effectiveFormantSemitones = juce::jlimit (-36.0f, 36.0f, effectiveFormantSemitones);

    currentPitchSemitones = totalPitchSemitones;
    currentFormantSemitones = effectiveFormantSemitones;

    const float pitchRatio = std::pow (2.0f, totalPitchSemitones / 12.0f);
    const float formantRatio = std::pow (2.0f, effectiveFormantSemitones / 12.0f);

    float mixPercent = pMix->load();
    float drivePercent = pDrive->load();
#if EMOBOY_NERD_FEATURES
    mixPercent += modMatrix.sumFor ((int) Param::Target::Mix) * kMixModRangePercent;
    drivePercent += modMatrix.sumFor ((int) Param::Target::Drive) * kDriveModRangePercent;
#endif
    mixPercent = juce::jlimit (0.0f, 100.0f, mixPercent);
    drivePercent = juce::jlimit (0.0f, 100.0f, drivePercent);
    // 0-2 internally: the old 0-100% mapped straight to Drive::processSample's
    // 0-1 range; the knob's top half now pushes past that (2026-08-20 rescale,
    // see Drive.h) - fader at 50% reproduces the pre-rescale 100% sound
    // exactly, 100% is twice as far again.
    const float driveAmount = (drivePercent / 100.0f) * 2.0f;

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

    // ---- auto gain: match the wet signal's loudness back to the dry ----
    // reference, after every effect but before Mix - see AutoGain.h.
    {
        float* wetPtrs[8];
        const float* dryPtrs[8];
        const int wetChans = juce::jmin (numChannels, 8);
        const int dryChans = juce::jmin (dryBuffer.getNumChannels(), 8);
        for (int c = 0; c < wetChans; ++c) wetPtrs[c] = buffer.getWritePointer (c);
        for (int c = 0; c < dryChans; ++c) dryPtrs[c] = dryBuffer.getReadPointer (c);
        autoGain.process (wetPtrs, wetChans, dryPtrs, dryChans, numSamples);
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

void EmoBoyProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= (int) Presets::table.size())
        return;

    currentProgram = index;
    programWasExplicitlySet = true;

    const auto& p = Presets::table[(size_t) index];
    auto setParam = [this] (const juce::String& id, float value)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };
    setParam (Param::pitch, p.pitch);
    setParam (Param::formant, p.formant);
    setParam (Param::drive, p.drive);
    setParam (Param::mix, p.mix);
    setParam (Param::mode, (float) (int) p.mode);
    setParam (Param::robotNote, (float) p.robotNoteIndex);
}

namespace
{
    constexpr const char* kUserPresetsNodeName = "UserPresets";
    constexpr const char* kUserPresetSlotName = "Slot";
}

void EmoBoyProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (! state.isValid())
        return;

    // Belt-and-braces: copyState() returns apvts's own tree, which may
    // already carry a UserPresets child left over from a previous
    // setStateInformation() call (see below) - drop it before appending a
    // fresh one so these don't pile up across repeated save/load cycles.
    state.removeChild (state.getChildWithName (kUserPresetsNodeName), nullptr);

    juce::ValueTree presetsNode (kUserPresetsNodeName);
    for (const auto& slot : userPresets)
    {
        juce::ValueTree slotNode (kUserPresetSlotName);
        slotNode.setProperty ("hasData", slot.hasData, nullptr);
        slotNode.setProperty ("pitch", slot.pitch, nullptr);
        slotNode.setProperty ("formant", slot.formant, nullptr);
        slotNode.setProperty ("drive", slot.drive, nullptr);
        slotNode.setProperty ("mix", slot.mix, nullptr);
        slotNode.setProperty ("mode", slot.mode, nullptr);
        slotNode.setProperty ("robotNoteIndex", slot.robotNoteIndex, nullptr);
        presetsNode.appendChild (slotNode, nullptr);
    }
    state.appendChild (presetsNode, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void EmoBoyProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto newState = juce::ValueTree::fromXml (*xml);

    if (auto presetsNode = newState.getChildWithName (kUserPresetsNodeName); presetsNode.isValid())
    {
        for (int i = 0; i < (int) userPresets.size() && i < presetsNode.getNumChildren(); ++i)
        {
            const auto slotNode = presetsNode.getChild (i);
            auto& slot = userPresets[(size_t) i];
            slot.hasData = (bool) slotNode.getProperty ("hasData", false);
            slot.pitch = (float) slotNode.getProperty ("pitch", 0.0f);
            slot.formant = (float) slotNode.getProperty ("formant", 0.0f);
            slot.drive = (float) slotNode.getProperty ("drive", 0.0f);
            slot.mix = (float) slotNode.getProperty ("mix", 100.0f);
            slot.mode = (int) slotNode.getProperty ("mode", 0);
            slot.robotNoteIndex = (int) slotNode.getProperty ("robotNoteIndex", 12);
        }
        // Keep apvts's own state tree free of our extra node - it isn't a
        // parameter and there's no reason for APVTS's own bookkeeping to
        // carry it around.
        newState.removeChild (presetsNode, nullptr);
    }

    apvts.replaceState (newState);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EmoBoyProcessor();
}
