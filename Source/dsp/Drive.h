#pragma once

#include <cmath>

// ---------------------------------------------------------------------------
// Tube-flavoured saturation stage, sits after the pitch/formant engine and
// before the dry/wet mix. Not a port of anything from "Not Sure" - written
// fresh for this project as the brief asked.
//
// Recipe (a small, well-known one, in the spirit of what a "Decapitator
// style" saturator does): a light one-pole pre-emphasis tilts energy up
// into the shaper so saturation reacts more to presence than to low-end
// mass, an asymmetric tanh (even-harmonic-heavy, tube-like) does the actual
// clipping, and a one-pole de-emphasis afterwards tilts the tone back down
// so Drive changes grit without also changing brightness.
// ---------------------------------------------------------------------------
class Drive
{
public:
    void prepare (double sampleRateIn)
    {
        sampleRate = sampleRateIn;
        preState = 0.0f;
        deState = 0.0f;

        constexpr float pi = 3.14159265358979323846f;
        const float cutoff = 800.0f;
        // Leaky-integrator coefficient shared by the pre/de-emphasis pair -
        // not an exact analytic inverse of each other, but a matched
        // boost/cut around the same corner, which is all this needs to be.
        filterCoeff = std::exp (-2.0f * pi * cutoff / (float) sampleRate);
    }

    // amount: 0..2 - the Drive parameter maps 0-50% to 0-1 here and
    // 50-100% to 1-2, so what used to be the full-drive sound at the old
    // 100% now sits at 50%, and the top half of the knob pushes twice as
    // far (2026-08-20, user's own ask, calibrated by ear against the
    // pre-rescale build - not re-derived here, just extended past 1.0).
    float processSample (float x, float amount)
    {
        if (amount <= 0.0001f)
            return x;

        // The dry/wet crossfade at the very end is deliberately clamped to
        // 1 (fully wet) even past amount=1 - letting it keep scaling would
        // extrapolate past the shaped signal itself (2*output - x territory),
        // which overshoots the tanh's own bound instead of driving harder
        // through it. Every other stage below keeps using the full,
        // unclamped amount, which is what actually delivers "twice as much"
        // character at the top of the range.
        const float blendAmount = amount < 1.0f ? amount : 1.0f;

        // Pre-emphasis: differentiator-style high-shelf boost.
        const float highPart = x - preState;
        preState += (1.0f - filterCoeff) * highPart;
        const float emphasised = x + amount * 1.2f * highPart;

        // Drive scales input gain into the shaper; a small positive bias
        // makes the transfer curve asymmetric so even harmonics appear -
        // symmetric tanh alone sounds sterile/transistor-like, the offset
        // is what reads as "tube".
        const float driveGain = 1.0f + amount * 9.0f;
        const float bias = amount * 0.15f;
        const float shaped = std::tanh (emphasised * driveGain + bias) - std::tanh (bias);

        // Output level compensation so higher Drive isn't just louder -
        // tanh's slope collapses towards saturation as driveGain grows.
        const float normaliser = std::tanh (driveGain * 0.6f + bias);
        const float compensated = normaliser > 1.0e-6f ? shaped / normaliser : shaped;

        // De-emphasis: gentle lowpass smoothing back towards the shape of
        // the pre-emphasis boost, so Drive changes grit more than tone.
        deState += (1.0f - filterCoeff) * (compensated - deState);
        const float output = deState + (compensated - deState) * (1.0f - amount * 0.5f);

        // Blend in proportion to (clamped) amount so Drive = 0 is exactly
        // transparent and the low end of the knob is a smooth ramp, not a
        // switch.
        return x + blendAmount * (output - x);
    }

private:
    double sampleRate = 44100.0;
    float filterCoeff = 0.9f;
    float preState = 0.0f;
    float deState = 0.0f;
};
