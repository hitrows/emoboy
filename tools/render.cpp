// ---------------------------------------------------------------------------
// Offline numeric verification tool for PitchFormantEngine. Not part of the
// plugin, not shipped - a fast way to check the DSP actually does what it
// claims (gain calibration, latency, pitch accuracy, formant/pitch
// orthogonality) without opening Logic and listening. Mirrors the workflow
// the "Not Sure" project uses (notsure-render): measure, don't assume.
// ---------------------------------------------------------------------------
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Source/dsp/PitchFormantEngine.h"
#include "../Source/dsp/PitchDetector.h"
#include <cstdio>
#include <vector>
#include <cmath>

namespace
{
    constexpr double kSampleRate = 44100.0;

    // A vowel-ish buzz: fundamental + harmonics shaped by two formant bumps,
    // so pitch shift (moves harmonic spacing) and formant shift (moves the
    // envelope bumps) are both independently measurable.
    std::vector<float> makeBuzz (double f0, double seconds, double formant1 = 800.0, double formant2 = 2200.0)
    {
        const int n = (int) (seconds * kSampleRate);
        std::vector<float> out ((size_t) n, 0.0f);
        const int numHarmonics = (int) (8000.0 / f0);

        for (int h = 1; h <= numHarmonics; ++h)
        {
            const double freq = f0 * h;
            const double bump1 = std::exp (-0.5 * std::pow ((std::log (freq) - std::log (formant1)) / 0.35, 2.0));
            const double bump2 = std::exp (-0.5 * std::pow ((std::log (freq) - std::log (formant2)) / 0.35, 2.0));
            const double amp = (0.15 + bump1 + 0.6 * bump2) / h * 0.3;

            for (int i = 0; i < n; ++i)
                out[(size_t) i] += (float) (amp * std::sin (2.0 * juce::MathConstants<double>::pi * freq * (double) i / kSampleRate));
        }
        return out;
    }

    double rms (const float* data, int n)
    {
        double sum = 0.0;
        for (int i = 0; i < n; ++i) sum += (double) data[i] * (double) data[i];
        return std::sqrt (sum / (double) juce::jmax (1, n));
    }

    // Run the engine on a mono buffer, block-by-block like a real host would.
    std::vector<float> runEngine (PitchFormantEngine& engine, const std::vector<float>& input, float pitchRatio, float formantRatio, int blockSize)
    {
        std::vector<float> output (input.size(), 0.0f);
        juce::AudioBuffer<float> block (1, blockSize);

        int pos = 0;
        while (pos < (int) input.size())
        {
            const int n = juce::jmin (blockSize, (int) input.size() - pos);
            block.setSize (1, n, false, false, true);
            block.copyFrom (0, 0, input.data() + pos, n);
            engine.process (block, pitchRatio, formantRatio);
            std::copy (block.getReadPointer (0), block.getReadPointer (0) + n, output.begin() + pos);
            pos += n;
        }
        return output;
    }
}

int main (int argc, char** argv)
{
    juce::ignoreUnused (argc, argv);

    PitchFormantEngine engine;
    engine.prepare (kSampleRate, 1);
    const int latency = engine.getLatencySamples();
    printf ("Reported latency: %d samples (%.1f ms)\n", latency, 1000.0 * latency / kSampleRate);

    // ---- Test 1: identity gain calibration -----------------------------
    {
        auto tone = makeBuzz (180.0, 1.5);
        auto out = runEngine (engine, tone, 1.0f, 1.0f, 512);
        const int settle = latency + 4096;
        const double inR = rms (tone.data() + settle, (int) tone.size() - settle);
        const double outR = rms (out.data() + settle, (int) out.size() - settle);
        printf ("[identity] input RMS=%.8f output RMS=%.8f ratio=%.8f (want ~1.0)\n", inR, outR, outR / inR);
    }

    // ---- Test 2: pitch accuracy, formant held at 0 ----------------------
    auto testPitch = [&] (double f0, float semitones)
    {
        engine.reset();
        auto tone = makeBuzz (f0, 1.5);
        const float ratio = std::pow (2.0f, semitones / 12.0f);
        auto out = runEngine (engine, tone, ratio, 1.0f, 512);

        PitchDetector det;
        det.prepare (kSampleRate, 2048);
        const int settle = latency + 4096;
        det.pushBlock (out.data() + settle, (int) out.size() - settle);

        const double expected = f0 * (double) ratio;
        printf ("[pitch %+.1fst] f0=%.1fHz expected=%.2fHz detected=%.2fHz voiced=%d\n",
                semitones, f0, expected, (double) det.getFrequencyHz(), (int) det.isVoiced());
    };
    testPitch (180.0, 7.0f);
    testPitch (180.0, -5.0f);
    testPitch (140.0, 12.0f);

    // ---- Test 3: formant shift moves the spectral envelope, pitch fixed -
    auto measureSpectrum = [&] (const std::vector<float>& signal, int settle, double loHz, double hiHz, double& centroidOut, double& peakOut)
    {
        constexpr int N = 8192;
        std::vector<std::complex<float>> buf (N), out (N);
        juce::dsp::FFT fft (13); // 2^13 = 8192
        for (int i = 0; i < N; ++i)
        {
            const float w = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) N);
            buf[(size_t) i] = { signal[(size_t) (settle + i)] * w, 0.0f };
        }
        fft.perform (buf.data(), out.data(), false);

        double num = 0.0, den = 0.0, peakMag = -1.0, peakFreq = 0.0;
        for (int k = 1; k < N / 2; ++k)
        {
            const double freq = (double) k * kSampleRate / (double) N;
            if (freq < loHz || freq > hiHz) continue;
            const double mag = std::abs (out[(size_t) k]);
            num += freq * mag;
            den += mag;
            if (mag > peakMag) { peakMag = mag; peakFreq = freq; }
        }
        centroidOut = den > 0.0 ? num / den : 0.0;
        peakOut = peakFreq;
    };

    // Diagnostic sweep - only meaningful if someone is re-tuning the
    // cutoff; the shipped engine always uses prepare()'s own formula.
    for (int cutoff : { 28, 31, 34, 37, 40, 43, 46 })
    {
        engine.reset();
        engine.setCepstrumCutoffForTesting (cutoff);
        auto tone = makeBuzz (110.0, 1.5);
        auto outBase = runEngine (engine, tone, 1.0f, 1.0f, 512);
        engine.reset();
        engine.setCepstrumCutoffForTesting (cutoff);
        auto outShifted = runEngine (engine, tone, 1.0f, std::pow (2.0f, 7.0f / 12.0f), 512);

        const int settle = latency + 4096;
        double centroidBase, peakBase, centroidShifted, peakShifted;
        measureSpectrum (outBase, settle, 400.0, 1400.0, centroidBase, peakBase);
        measureSpectrum (outShifted, settle, 400.0, 1800.0, centroidShifted, peakShifted);

        PitchDetector det;
        det.prepare (kSampleRate, 2048);
        det.pushBlock (outShifted.data() + settle, (int) outShifted.size() - settle);

        printf ("[sweep cutoff=%3d] base peak=%.1fHz | shifted peak=%.1fHz (target ~1198Hz) | f0 detected=%.2fHz (want ~110Hz)\n",
                cutoff, peakBase, peakShifted, (double) det.getFrequencyHz());
    }

    // Final check with the engine exactly as the plugin configures it
    // (prepare()'s own cutoff formula, no override).
    {
        PitchFormantEngine fresh;
        fresh.prepare (kSampleRate, 1);
        auto tone = makeBuzz (110.0, 1.5);
        auto outBase = runEngine (fresh, tone, 1.0f, 1.0f, 512);
        fresh.reset();
        auto outShifted = runEngine (fresh, tone, 1.0f, std::pow (2.0f, 7.0f / 12.0f), 512);

        const int settle = latency + 4096;
        double centroidBase, peakBase, centroidShifted, peakShifted;
        measureSpectrum (outBase, settle, 400.0, 1400.0, centroidBase, peakBase);
        measureSpectrum (outShifted, settle, 400.0, 1800.0, centroidShifted, peakShifted);
        printf ("[shipped default] base peak=%.1fHz | shifted (+7st formant) peak=%.1fHz (target ~1198Hz)\n", peakBase, peakShifted);
    }

    return 0;
}
