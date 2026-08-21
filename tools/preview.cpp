// ---------------------------------------------------------------------------
// Offscreen editor snapshot tool - renders EmoBoyEditor to PNG files without
// opening a window. Built because screen-recording permission wasn't
// available in this session to just screenshot the real app, and "does the
// pink line actually land on the fader" is not something worth guessing at
// from pixel arithmetic alone. Not shipped, not part of the plugin.
// ---------------------------------------------------------------------------
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>
#include <cmath>
#include <vector>

namespace
{
    void setParam (EmoBoyProcessor& proc, const juce::String& id, float actualValue)
    {
        if (auto* param = proc.apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (actualValue));
    }

    void snapshot (EmoBoyProcessor& proc, const char* filename)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        editor->setVisible (true);

        // Let SliderAttachment's async parameter->UI updates flush before
        // we grab pixels - they're dispatched via the message loop, not
        // applied synchronously inside setValueNotifyingHost().
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

        auto image = editor->createComponentSnapshot (editor->getLocalBounds());
        juce::File outFile (juce::File::getCurrentWorkingDirectory().getChildFile (filename));
        juce::PNGImageFormat png;
        juce::FileOutputStream stream (outFile);
        png.writeImageToStream (image, stream);
        printf ("wrote %s (%dx%d)\n", filename, image.getWidth(), image.getHeight());
    }
}

namespace
{
    double rms (const float* data, int n)
    {
        double sum = 0.0;
        for (int i = 0; i < n; ++i) sum += (double) data[i] * (double) data[i];
        return std::sqrt (sum / (double) juce::jmax (1, n));
    }

    // Runs a mono sine through the full processor (auto gain included,
    // Mix forced to 100% so the wet path is all that's measured) and
    // compares output loudness back to input loudness. 2026-08-20: added
    // to check the new AutoGain stage actually equalises level rather
    // than assuming the math is right from reading it.
    void checkAutoGain()
    {
        constexpr double sr = 44100.0;
        constexpr int blockSize = 512;
        constexpr double seconds = 2.0;
        const int n = (int) (seconds * sr);

        std::vector<float> tone ((size_t) n);
        for (int i = 0; i < n; ++i)
            tone[(size_t) i] = 0.4f * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * i / sr);
        const double inputRms = rms (tone.data(), n);

        struct Case { float pitch, drive; const char* label; };
        const Case cases[] = {
            { 0.0f, 0.0f, "pitch=0 drive=0" },
            { 0.0f, 50.0f, "pitch=0 drive=50" },
            { 0.0f, 100.0f, "pitch=0 drive=100" },
            { 7.0f, 100.0f, "pitch=+7 drive=100" },
            { -7.0f, 25.0f, "pitch=-7 drive=25" },
        };

        for (const auto& c : cases)
        {
            EmoBoyProcessor proc;
            proc.prepareToPlay (sr, blockSize);
            setParam (proc, Param::pitch, c.pitch);
            setParam (proc, Param::drive, c.drive);
            setParam (proc, Param::mix, 100.0f);

            std::vector<float> output ((size_t) n, 0.0f);
            juce::AudioBuffer<float> block (1, blockSize);
            juce::MidiBuffer midi;
            int pos = 0;
            while (pos < n)
            {
                const int bs = juce::jmin (blockSize, n - pos);
                block.setSize (1, bs, false, false, true);
                block.copyFrom (0, 0, tone.data() + pos, bs);
                proc.processBlock (block, midi);
                std::copy (block.getReadPointer (0), block.getReadPointer (0) + bs, output.begin() + pos);
                pos += bs;
            }

            const int settle = proc.getLatencySamples() + 8192;
            const double outputRms = settle < n ? rms (output.data() + settle, n - settle) : 0.0;
            const double errorDb = 20.0 * std::log10 (juce::jmax (1.0e-9, outputRms) / juce::jmax (1.0e-9, inputRms));
            printf ("[autogain %-20s] input RMS=%.4f output RMS=%.4f  error=%.2f dB\n",
                    c.label, inputRms, outputRms, errorDb);
        }
    }

    // Confirms the BYPASS footswitch actually leaves the signal untouched
    // (bit-identical, not just "close") - 2026-08-20.
    void checkBypass()
    {
        constexpr double sr = 44100.0;
        constexpr int blockSize = 512;
        constexpr int n = 4096;

        std::vector<float> tone ((size_t) n);
        for (int i = 0; i < n; ++i)
            tone[(size_t) i] = 0.4f * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * i / sr);

        EmoBoyProcessor proc;
        proc.prepareToPlay (sr, blockSize);
        setParam (proc, Param::bypass, 1.0f);
        // Heavy settings that would obviously change the signal if bypass
        // weren't actually skipping processing.
        setParam (proc, Param::pitch, 12.0f);
        setParam (proc, Param::drive, 100.0f);

        std::vector<float> output ((size_t) n, 0.0f);
        juce::AudioBuffer<float> block (1, blockSize);
        juce::MidiBuffer midi;
        int pos = 0;
        while (pos < n)
        {
            const int bs = juce::jmin (blockSize, n - pos);
            block.setSize (1, bs, false, false, true);
            block.copyFrom (0, 0, tone.data() + pos, bs);
            proc.processBlock (block, midi);
            std::copy (block.getReadPointer (0), block.getReadPointer (0) + bs, output.begin() + pos);
            pos += bs;
        }

        double maxDiff = 0.0;
        for (int i = 0; i < n; ++i)
            maxDiff = juce::jmax (maxDiff, (double) std::abs (output[(size_t) i] - tone[(size_t) i]));
        printf ("[bypass] max |output - input| = %.8f (want exactly 0.0)\n", maxDiff);
    }

    // Confirms the PEAK lamp actually snaps on for a loud input and stays
    // off for silence/quiet input - 2026-08-20.
    void checkPeakLamp()
    {
        constexpr double sr = 44100.0;
        constexpr int blockSize = 512;

        auto runFor = [&] (float amplitude, int numSamples) -> bool
        {
            EmoBoyProcessor proc;
            proc.prepareToPlay (sr, blockSize);
            juce::AudioBuffer<float> block (1, blockSize);
            juce::MidiBuffer midi;
            int pos = 0;
            while (pos < numSamples)
            {
                const int bs = juce::jmin (blockSize, numSamples - pos);
                block.setSize (1, bs, false, false, true);
                for (int i = 0; i < bs; ++i)
                    block.setSample (0, i, amplitude * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (pos + i) / sr));
                proc.processBlock (block, midi);
                pos += bs;
            }
            return proc.isPeakLedOn();
        };

        printf ("[peak lamp] loud (0.5) input -> lit=%d (want 1)\n", (int) runFor (0.5f, 4096));
        printf ("[peak lamp] silent input -> lit=%d (want 0)\n", (int) runFor (0.0f, 4096));
        printf ("[peak lamp] quiet (0.01) input -> lit=%d (want 0)\n", (int) runFor (0.01f, 4096));
    }

    // Confirms the WRITE-button user preset slots (2026-08-21) actually
    // survive a getStateInformation -> setStateInformation round trip -
    // the exact path a real project save/reload takes. Sets slots 0 and 2
    // directly (as EmoBoyEditor::onUserSlotClicked would), leaves 1 and 3
    // empty, and checks a fresh processor loading that state ends up with
    // identical data - including the empty slots staying empty (hasData
    // must not default to true from a missing/mis-parsed property).
    void checkUserPresetPersistence()
    {
        EmoBoyProcessor writer;
        writer.prepareToPlay (44100.0, 512);

        auto& slot0 = writer.userPresets[0];
        slot0.hasData = true;
        slot0.pitch = 7.0f; slot0.formant = -3.0f; slot0.drive = 42.0f; slot0.mix = 80.0f;
        slot0.mode = (int) Param::Mode::Robot; slot0.robotNoteIndex = 18;

        auto& slot2 = writer.userPresets[2];
        slot2.hasData = true;
        slot2.pitch = -12.0f; slot2.formant = 5.0f; slot2.drive = 0.0f; slot2.mix = 100.0f;
        slot2.mode = (int) Param::Mode::Transpose; slot2.robotNoteIndex = 3;

        juce::MemoryBlock state;
        writer.getStateInformation (state);

        EmoBoyProcessor reader;
        reader.prepareToPlay (44100.0, 512);
        reader.setStateInformation (state.getData(), (int) state.getSize());

        bool ok = true;
        ok &= reader.userPresets[0].hasData == true
           && reader.userPresets[0].pitch == 7.0f && reader.userPresets[0].formant == -3.0f
           && reader.userPresets[0].drive == 42.0f && reader.userPresets[0].mix == 80.0f
           && reader.userPresets[0].mode == (int) Param::Mode::Robot && reader.userPresets[0].robotNoteIndex == 18;
        ok &= reader.userPresets[1].hasData == false; // never written - must stay empty
        ok &= reader.userPresets[2].hasData == true
           && reader.userPresets[2].pitch == -12.0f && reader.userPresets[2].formant == 5.0f
           && reader.userPresets[2].drive == 0.0f && reader.userPresets[2].mix == 100.0f
           && reader.userPresets[2].mode == (int) Param::Mode::Transpose && reader.userPresets[2].robotNoteIndex == 3;
        ok &= reader.userPresets[3].hasData == false;

        printf ("[user presets] round-trip through save/load state -> %s\n", ok ? "OK" : "MISMATCH");
    }

    // Confirms FREEZE actually substitutes a looped snapshot for the
    // engine's input, rather than e.g. just muting or passing live input
    // through unchanged (2026-08-21). Feeds a real tone with FREEZE off,
    // engages it, then feeds SILENCE for many loop-lengths - if FREEZE
    // is doing nothing the output should go silent; if it's working the
    // output should keep sounding like the tone, repeating every
    // freezeLoopSamples once things settle past the engine's own analysis
    // latency. Loop shortened from an initial 500ms to 50ms (2026-08-21 -
    // 500ms was audible as a repeating phrase, not a held note; see
    // PluginProcessor.cpp's own comment on freezeLoopSamples).
    void checkFreeze()
    {
        constexpr double sr = 44100.0;
        constexpr int blockSize = 512;
        const int loopSamples = (int) (0.05 * sr); // must match PluginProcessor's own freezeLoopSamples

        EmoBoyProcessor proc;
        proc.prepareToPlay (sr, blockSize);
        setParam (proc, Param::mix, 100.0f);

        int phaseSamples = 0;
        auto runBlocks = [&] (int numSamples, bool tone, std::vector<float>* capture)
        {
            juce::AudioBuffer<float> block (1, blockSize);
            juce::MidiBuffer midi;
            int pos = 0;
            while (pos < numSamples)
            {
                const int bs = juce::jmin (blockSize, numSamples - pos);
                block.setSize (1, bs, false, false, true);
                for (int i = 0; i < bs; ++i)
                    block.setSample (0, i, tone ? 0.3f * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (phaseSamples + i) / sr) : 0.0f);
                proc.processBlock (block, midi);
                if (capture != nullptr)
                    for (int i = 0; i < bs; ++i)
                        capture->push_back (block.getSample (0, i));
                pos += bs;
                phaseSamples += bs;
            }
        };

        // Settle the engine on a real tone first (also populates the
        // freeze ring with real material to snapshot). Needs to be well
        // past the engine's own 2048-sample analysis window, not just a
        // couple of these now-tiny 50ms loops.
        runBlocks (proc.getLatencySamples() * 3, true, nullptr);

        setParam (proc, Param::freeze, 1.0f);

        // Feed silence for many loop-lengths with FREEZE on - capture it
        // all. With a 50ms loop this is still under a second of audio.
        std::vector<float> frozenOutput;
        runBlocks (loopSamples * 40, false, &frozenOutput);

        // Skip the engine's own analysis latency plus several loops so
        // the (now much shorter) periodic input has time to actually
        // settle into a steady spectral state, then compare two windows
        // exactly one loop-length apart (both comfortably inside the
        // 40-loop capture).
        const int settle = proc.getLatencySamples() + loopSamples * 10;
        const int compareLen = loopSamples - 128;
        double maxDiff = 0.0, sumSq = 0.0;
        for (int i = 0; i < compareLen; ++i)
        {
            const float a = frozenOutput[(size_t) (settle + i)];
            const float b = frozenOutput[(size_t) (settle + loopSamples + i)];
            maxDiff = juce::jmax (maxDiff, (double) std::abs (a - b));
            sumSq += (double) a * a;
        }
        const double rms = std::sqrt (sumSq / compareLen);
        // Not asserting near-zero on the diff: a phase vocoder's running
        // phase accumulator isn't guaranteed to return to *exactly* the
        // same state after one loop even when its input is perfectly
        // periodic, so a small residual is expected. What actually
        // matters here (and what this checks): the output is clearly NOT
        // silence while frozen (proving substitution is happening, not
        // muting) and the two loop-apart windows are close relative to
        // the signal's own scale (proving it's substantially the same
        // material repeating, not fresh/unrelated content each cycle).
        printf ("[freeze] output RMS while frozen=%.4f (not near 0 = substitution is happening), "
                "one-loop-apart max diff=%.5f (small relative to RMS = same material repeating)\n", rms, maxDiff);
    }

    // Confirms engaging/releasing FREEZE doesn't click (2026-08-21,
    // self-review finding: the original implementation substituted the
    // live<->loop buffers instantly, with no crossfade at the switch
    // itself - only the loop's own internal seam was smoothed). Compares
    // the largest sample-to-sample jump in a small window straddling each
    // transition instant against the largest jump seen during ordinary
    // steady-state playback of the same tone; an un-crossfaded switch
    // would show up here as a spike far above the steady-state baseline.
    void checkFreezeTransitionClicks()
    {
        constexpr double sr = 44100.0;
        constexpr int blockSize = 512;
        const int loopSamples = (int) (0.05 * sr);

        EmoBoyProcessor proc;
        proc.prepareToPlay (sr, blockSize);
        setParam (proc, Param::mix, 100.0f);

        std::vector<float> output;
        int phaseSamples = 0;
        auto runBlocks = [&] (int numSamples)
        {
            juce::AudioBuffer<float> block (1, blockSize);
            juce::MidiBuffer midi;
            int pos = 0;
            while (pos < numSamples)
            {
                const int bs = juce::jmin (blockSize, numSamples - pos);
                block.setSize (1, bs, false, false, true);
                for (int i = 0; i < bs; ++i)
                    block.setSample (0, i, 0.3f * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (phaseSamples + i) / sr));
                proc.processBlock (block, midi);
                for (int i = 0; i < bs; ++i)
                    output.push_back (block.getSample (0, i));
                pos += bs;
                phaseSamples += bs;
            }
        };

        runBlocks (proc.getLatencySamples() * 3); // settle on live tone first
        const int engageIndex = (int) output.size();
        setParam (proc, Param::freeze, 1.0f);
        runBlocks (loopSamples * 4); // hold frozen a while
        const int releaseIndex = (int) output.size();
        setParam (proc, Param::freeze, 0.0f);
        runBlocks (loopSamples * 4); // back to live

        auto maxJumpIn = [&] (int from, int to)
        {
            double m = 0.0;
            for (int i = juce::jmax (1, from); i < to; ++i)
                m = juce::jmax (m, (double) std::abs (output[(size_t) i] - output[(size_t) (i - 1)]));
            return m;
        };

        const double steadyState = maxJumpIn (256, engageIndex - 256);
        const double atEngage = maxJumpIn (engageIndex - 32, engageIndex + 32);
        const double atRelease = maxJumpIn (releaseIndex - 32, releaseIndex + 32);

        printf ("[freeze transitions] steady-state max sample jump=%.5f, at engage=%.5f, at release=%.5f "
                "(want engage/release not much bigger than steady-state - a click would spike far above it)\n",
                steadyState, atEngage, atRelease);
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiser;

    checkAutoGain();
    checkPeakLamp();
    checkUserPresetPersistence();
    checkFreeze();
    checkFreezeTransitionClicks();

    {
        EmoBoyProcessor proc;
        proc.prepareToPlay (44100.0, 512);
        // Defaults: pitch=0, formant=0, drive=0, mix=100 - sanity check the
        // resting position first.
        snapshot (proc, "preview_default.png");
    }

    {
        EmoBoyProcessor proc;
        proc.prepareToPlay (44100.0, 512);
        juce::AudioBuffer<float> block (1, 512);
        juce::MidiBuffer midi;
        for (int i = 0; i < 512; ++i)
            block.setSample (0, i, 0.5f * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * i / 44100.0));
        proc.processBlock (block, midi); // pump one loud block so the PEAK lamp is lit for the snapshot
        snapshot (proc, "preview_peak_lit.png");
    }

    {
        EmoBoyProcessor proc;
        proc.prepareToPlay (44100.0, 512);
        setParam (proc, Param::pitch, 12.0f);
        setParam (proc, Param::formant, -12.0f);
        setParam (proc, Param::drive, 100.0f);
        setParam (proc, Param::mix, 0.0f);
        snapshot (proc, "preview_extremes.png");
    }

    // Preset parameter values (2026-08-20) - EmoBoyEditor::applyPreset()
    // isn't reachable from here (private, and createEditor() only exposes
    // the base AudioProcessorEditor*), so this replicates what it sets
    // via the same public parameter path, to confirm fader positions land
    // where the presets table in PluginEditor.cpp says they should.
    {
        EmoBoyProcessor proc;
        proc.prepareToPlay (44100.0, 512);
        setParam (proc, Param::pitch, -3.0f);
        setParam (proc, Param::formant, -3.0f);
        setParam (proc, Param::drive, 15.0f);
        setParam (proc, Param::mix, 100.0f);
        snapshot (proc, "preview_preset_cry.png");
    }

    {
        EmoBoyProcessor proc;
        proc.prepareToPlay (44100.0, 512);
        setParam (proc, Param::mode, (float) (int) Param::Mode::Robot);
        setParam (proc, Param::formant, 2.0f);
        setParam (proc, Param::drive, 35.0f);
        setParam (proc, Param::mix, 100.0f);
        snapshot (proc, "preview_preset_robot.png");
    }

    {
        EmoBoyProcessor proc;
        proc.prepareToPlay (44100.0, 512);
        setParam (proc, Param::pitch, 6.0f);
        setParam (proc, Param::formant, -6.0f);
        setParam (proc, Param::drive, 50.0f);
        setParam (proc, Param::mix, 50.0f);
        snapshot (proc, "preview_mid.png");
    }

    {
        EmoBoyProcessor proc;
        proc.prepareToPlay (44100.0, 512);
        setParam (proc, Param::bypass, 1.0f);
        // HITROWS wordmark should be dark here (only unlit look from the
        // background photo, no glow overlay), unlike every other snapshot
        // in this file where bypass=0 and it should be lit - 2026-08-20.
        snapshot (proc, "preview_bypassed.png");
    }

    {
        EmoBoyProcessor proc;
        proc.prepareToPlay (44100.0, 512);
        setParam (proc, Param::mode, (float) (int) Param::Mode::Quantize);
        snapshot (proc, "preview_mode_quantize.png"); // mode button "2" should be lit
    }

    {
        EmoBoyProcessor proc;
        proc.prepareToPlay (44100.0, 512);
        setParam (proc, Param::mode, (float) (int) Param::Mode::Robot);
        setParam (proc, Param::robotNote, 0.0f); // index 0, bottom of the list -> 7 o'clock (rework, 2026-08-20)
        snapshot (proc, "preview_robot_noteLow.png");
    }

    {
        EmoBoyProcessor proc;
        proc.prepareToPlay (44100.0, 512);
        setParam (proc, Param::mode, (float) (int) Param::Mode::Robot);
        // index 12 = reference note, left untouched -> should be default already, straight up (12 o'clock)
        snapshot (proc, "preview_robot_noteCentre_default.png");
    }

    {
        EmoBoyProcessor proc;
        proc.prepareToPlay (44100.0, 512);
        setParam (proc, Param::mode, (float) (int) Param::Mode::Robot);
        setParam (proc, Param::robotNote, 23.0f); // index 23, top of the list -> 5 o'clock
        snapshot (proc, "preview_robot_noteHigh.png");
    }

    checkBypass();

    return 0;
}
