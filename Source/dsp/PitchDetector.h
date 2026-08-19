#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// Fundamental-frequency detector, simplified YIN (de Cheveigne & Kawahara).
// Feed it hop-sized blocks of mono audio; every time it has accumulated a
// full analysis window it re-runs the estimate and reports it via
// getFrequencyHz()/isVoiced(). Used by Quantize mode, Robot mode (to know
// how far the input already is from the target note) and the PT modulation
// source.
//
// YIN in one paragraph: autocorrelation finds the lag that best repeats the
// waveform, but it also loves lag 0 and octave-double lags, which is why
// naive autocorrelation pitch trackers are unstable. YIN instead computes a
// "difference function" (how much the signal does NOT match itself at each
// lag), turns it into a cumulative mean-normalised version that suppresses
// the false low-lag matches, and picks the first dip below a threshold
// instead of the global minimum. That one change is most of why YIN is far
// more stable on real voices than plain autocorrelation.
// ---------------------------------------------------------------------------
class PitchDetector
{
public:
    void prepare (double sampleRateIn, int windowSizeIn)
    {
        sampleRate = sampleRateIn;
        windowSize = windowSizeIn;
        buffer.assign ((size_t) windowSize, 0.0f);
        difference.assign ((size_t) windowSize / 2, 0.0f);
        writePos = 0;
        filled = false;
        frequencyHz = 0.0f;
        voiced = false;
    }

    // Push one hop worth of samples in, re-estimate pitch once the internal
    // ring buffer has a full window.
    void pushBlock (const float* samples, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            buffer[(size_t) writePos] = samples[i];
            writePos = (writePos + 1) % windowSize;
            if (writePos == 0)
                filled = true;
        }

        if (filled)
            estimate();
    }

    float getFrequencyHz() const { return frequencyHz; }
    bool  isVoiced() const       { return voiced; }

private:
    void estimate()
    {
        const int halfWindow = windowSize / 2;
        const float minFreq = 70.0f;   // low end of a chest voice
        const float maxFreq = 1000.0f; // covers falsetto / female head voice
        const int maxLag = std::clamp ((int) (sampleRate / minFreq), 2, halfWindow - 1);
        const int minLag = std::clamp ((int) (sampleRate / maxFreq), 2, halfWindow - 1);

        // Difference function d(tau) = sum (x[i] - x[i+tau])^2, read out of
        // the ring buffer starting at the oldest sample so indices are
        // contiguous in time.
        for (int tau = 0; tau < halfWindow; ++tau)
        {
            double sum = 0.0;
            for (int i = 0; i < halfWindow; ++i)
            {
                const float a = buffer[(size_t) ((writePos + i) % windowSize)];
                const float b = buffer[(size_t) ((writePos + i + tau) % windowSize)];
                const double d = (double) a - (double) b;
                sum += d * d;
            }
            difference[(size_t) tau] = (float) sum;
        }

        // Cumulative mean normalised difference function.
        std::vector<float> cmnd ((size_t) halfWindow, 1.0f);
        double runningSum = 0.0;
        cmnd[0] = 1.0f;
        for (int tau = 1; tau < halfWindow; ++tau)
        {
            runningSum += difference[(size_t) tau];
            cmnd[(size_t) tau] = runningSum > 0.0
                ? (float) (difference[(size_t) tau] * tau / runningSum)
                : 1.0f;
        }

        const float threshold = 0.15f;
        int chosenLag = -1;
        for (int tau = minLag; tau < maxLag; ++tau)
        {
            if (cmnd[(size_t) tau] < threshold)
            {
                // Walk forward to the local minimum, then stop - this is the
                // "first dip", the part that keeps YIN from grabbing octave
                // errors the way plain autocorrelation does.
                while (tau + 1 < maxLag && cmnd[(size_t) (tau + 1)] < cmnd[(size_t) tau])
                    ++tau;
                chosenLag = tau;
                break;
            }
        }

        if (chosenLag < 0)
        {
            voiced = false;
            return;
        }

        // Parabolic interpolation around the chosen lag for sub-sample
        // precision - without this, pitch estimates snap to whole-sample
        // lag boundaries and quantize/robot mode audibly stairsteps.
        float betterLag = (float) chosenLag;
        if (chosenLag > 0 && chosenLag < halfWindow - 1)
        {
            const float s0 = cmnd[(size_t) (chosenLag - 1)];
            const float s1 = cmnd[(size_t) chosenLag];
            const float s2 = cmnd[(size_t) (chosenLag + 1)];
            const float denom = (2.0f * (s0 - 2.0f * s1 + s2));
            if (std::abs (denom) > 1.0e-9f)
                betterLag = (float) chosenLag + (s0 - s2) / denom * 0.5f;
        }

        frequencyHz = (float) (sampleRate / betterLag);
        voiced = true;
    }

    double sampleRate = 44100.0;
    int windowSize = 2048;
    std::vector<float> buffer;
    std::vector<float> difference;
    int writePos = 0;
    bool filled = false;
    float frequencyHz = 0.0f;
    bool voiced = false;
};
