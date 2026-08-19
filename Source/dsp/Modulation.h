#pragma once

#include <cmath>
#include <array>
#include <algorithm>

// ---------------------------------------------------------------------------
// Modulation sources (Mod1/Mod2 LFOs, AM envelope follower, PT pitch
// tracker) and the routing matrix that connects them to targets.
//
// Every source publishes a value in [-1, 1] once per block (control rate,
// not per-sample - a vocal effect's modulation targets are all
// slowly-varying by nature, so control-rate is inaudible as steps and much
// cheaper). AM is naturally one-sided (an envelope can't go negative) but
// is still reported through the same [-1, 1] slot so the routing matrix
// doesn't need special cases; its resting value is -1 (silence -> minimum),
// not 0.
// ---------------------------------------------------------------------------

class LFO
{
public:
    void prepare (double sampleRateIn) { sampleRate = sampleRateIn; }

    void setPhaseOffsetDegrees (float degrees) { phaseOffset = degrees / 360.0f; }

    // Advances by numSamples and returns the current value in [-1, 1].
    float process (int numSamples, float rateHz)
    {
        phase += (float) (rateHz * numSamples / sampleRate);
        phase -= std::floor (phase);
        const float p = phase + phaseOffset;
        return std::sin (p * 2.0f * 3.14159265358979323846f);
    }

private:
    double sampleRate = 44100.0;
    float phase = 0.0f;
    float phaseOffset = 0.0f;
};

class EnvelopeFollower
{
public:
    void prepare (double sampleRateIn) { sampleRate = sampleRateIn; }

    // Feed it a block, get back the envelope value in [0, 1] at block end,
    // remapped by the caller into [-1, 1] for the routing matrix.
    float process (const float* samples, int numSamples, float attackMs, float releaseMs)
    {
        const float attackCoeff  = coeffFromMs (attackMs);
        const float releaseCoeff = coeffFromMs (releaseMs);

        for (int i = 0; i < numSamples; ++i)
        {
            const float rectified = std::abs (samples[i]);
            const float coeff = rectified > envelope ? attackCoeff : releaseCoeff;
            envelope += (1.0f - coeff) * (rectified - envelope);
        }
        return juce_clamp01 (envelope);
    }

private:
    static float juce_clamp01 (float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    float coeffFromMs (float ms) const
    {
        const float t = std::max (0.1f, ms) * 0.001f * (float) sampleRate;
        return std::exp (-1.0f / t);
    }

    double sampleRate = 44100.0;
    float envelope = 0.0f;
};

// ---------------------------------------------------------------------------
// Routing matrix. Holds the current [-1, 1] value of each of the four
// sources and computes the summed modulation for each of the four targets
// given the on/off + depth state read from the APVTS each block.
// ---------------------------------------------------------------------------
struct ModMatrix
{
    static constexpr int numSources = 4; // Mod1, Mod2, AM, PT
    static constexpr int numTargets = 4; // Pitch, Formant, Mix, Drive

    std::array<float, numSources> sourceValue { { 0, 0, 0, 0 } };
    std::array<std::array<bool, numTargets>, numSources> enabled { };
    std::array<std::array<float, numTargets>, numSources> depth { }; // -1..1

    // Returns summed bipolar modulation in roughly [-1, 1] (can exceed if
    // multiple sources stack fully - callers scale by their own target
    // range and clamp the final musical value, not this).
    float sumFor (int targetIndex) const
    {
        float sum = 0.0f;
        for (int s = 0; s < numSources; ++s)
            if (enabled[(size_t) s][(size_t) targetIndex])
                sum += sourceValue[(size_t) s] * depth[(size_t) s][(size_t) targetIndex];
        return sum;
    }
};
