#pragma once

#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// Loudness-matching makeup gain (2026-08-20, user's own ask). Runs last in
// the wet path - after the pitch/formant engine and Drive, before the
// dry/wet Mix control - so Mix keeps meaning "how much character", not
// "how much extra loudness Drive/Pitch happened to add".
//
// Measures a slow RMS envelope of both the wet (post-effects) signal and
// the dry (pre-effects, already delay-compensated) reference signal, and
// applies a smoothed correction gain so the wet signal's overall level
// tracks the dry signal's. Always on - no bypass parameter exists yet; if
// that turns out to be wanted, this is where a "disabled" flag would short-
// circuit process() to a no-op.
// ---------------------------------------------------------------------------
class AutoGain
{
public:
    void prepare (double sampleRateIn)
    {
        sampleRate = sampleRateIn;
        rmsCoeff = coeffFromMs (300.0f);  // how quickly the loudness estimate itself tracks - a "how loud has this been lately" window, not a peak detector
        gainCoeff = coeffFromMs (80.0f);  // how quickly the correction gain moves - faster than the RMS window so it doesn't lag behind real level changes, slow enough to stay click-free
        reset();
    }

    void reset()
    {
        dryRmsSq = 0.0f;
        wetRmsSq = 0.0f;
        currentGain = 1.0f;
    }

    // wetChannels: the post-Drive signal, corrected in place.
    // dryChannels: the delay-compensated reference signal, read-only.
    // Channel counts may differ slightly; only the shared ones feed the
    // measurement, and the same (mono) correction gain is applied to every
    // wet channel so stereo balance never shifts.
    void process (float* const* wetChannels, int wetNumChannels,
                   const float* const* dryChannels, int dryNumChannels,
                   int numSamples)
    {
        const int measureChannels = std::min (wetNumChannels, dryNumChannels);
        if (measureChannels <= 0)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            float dryPower = 0.0f, wetPower = 0.0f;
            for (int c = 0; c < measureChannels; ++c)
            {
                const float d = dryChannels[c][i];
                const float w = wetChannels[c][i];
                dryPower += d * d;
                wetPower += w * w;
            }
            dryPower /= (float) measureChannels;
            wetPower /= (float) measureChannels;

            dryRmsSq += (1.0f - rmsCoeff) * (dryPower - dryRmsSq);
            wetRmsSq += (1.0f - rmsCoeff) * (wetPower - wetRmsSq);

            const float dryRms = std::sqrt (std::max (dryRmsSq, 1.0e-9f));
            const float wetRms = std::sqrt (std::max (wetRmsSq, 1.0e-9f));

            // Below this there's nothing meaningful to match - hold the
            // last gain instead of letting a near-silent denominator swing
            // the correction wildly (e.g. between phrases/breaths).
            constexpr float kFloor = 3.0e-4f; // ~ -70 dBFS
            if (wetRms > kFloor && dryRms > kFloor)
            {
                const float target = std::clamp (dryRms / wetRms, 0.177f, 5.62f); // +-15 dB
                currentGain += (1.0f - gainCoeff) * (target - currentGain);
            }

            for (int c = 0; c < wetNumChannels; ++c)
                wetChannels[c][i] *= currentGain;
        }
    }

private:
    float coeffFromMs (float ms) const
    {
        const float t = ms * 0.001f * (float) sampleRate;
        return std::exp (-1.0f / std::max (1.0f, t));
    }

    double sampleRate = 44100.0;
    float rmsCoeff = 0.999f;
    float gainCoeff = 0.998f;
    float dryRmsSq = 0.0f;
    float wetRmsSq = 0.0f;
    float currentGain = 1.0f;
};
