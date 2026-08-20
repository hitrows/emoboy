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
}

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiser;

    checkAutoGain();

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
        setParam (proc, Param::pitch, 12.0f);
        setParam (proc, Param::formant, -12.0f);
        setParam (proc, Param::drive, 100.0f);
        setParam (proc, Param::mix, 0.0f);
        snapshot (proc, "preview_extremes.png");
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

    return 0;
}
