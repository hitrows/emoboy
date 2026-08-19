#include "PitchFormantEngine.h"
#include <cmath>

namespace
{
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kTwoPi = 2.0f * kPi;

    // Bernsee/DSPdimension smbPitchShift derives its OLA normalisation for
    // his own unnormalised DFT; JUCE's FFT clearly scales differently
    // internally (measured output was ~1365x too quiet with this at 1.0).
    // Measured via tools/render.cpp's [identity] test (pitch=formant=1x
    // should reproduce input RMS) rather than re-derived analytically -
    // see HANDOFF.md, "engine gain calibration".
    constexpr float kSynthesisScale = 1364.16f;
}

void PitchFormantEngine::prepare (double sampleRateIn, int numChannels)
{
    sampleRate = sampleRateIn;
    channels.assign ((size_t) numChannels, ChannelState {});

    for (int k = 0; k < fftSize; ++k)
        analysisWindow[(size_t) k] = 0.5f - 0.5f * std::cos (kTwoPi * (float) k / (float) fftSize);

    // Cepstral liftering order: how many low-quefrency cepstrum samples are
    // kept as the formant envelope. NOT a textbook value - measured via
    // tools/render.cpp's cutoff sweep (see HANDOFF.md, "cepstral cutoff
    // calibration"). The textbook sampleRate/700 heuristic (63 samples at
    // 44.1kHz) measured as a near-total formant-shift failure on a 110Hz
    // test buzz - the excitation/envelope split wasn't clean enough and
    // most of the formant bump stayed baked into the "excitation" instead
    // of moving with the envelope. 37 samples at 44.1kHz was the measured
    // sweet spot; scaled proportionally with sample rate here since it is
    // a frequency-resolution quantity. The response is NOT smooth around
    // this value (neighbouring integers measured worse) - a known rough
    // edge of the simplified cepstral split, see HANDOFF.md.
    cepstrumCutoff = juce::jlimit (8, fftSize / 4, (int) std::lround (sampleRate * (37.0 / 44100.0)));

    reset();
}

void PitchFormantEngine::reset()
{
    for (auto& ch : channels)
    {
        std::fill (ch.inFifo.begin(), ch.inFifo.end(), 0.0f);
        std::fill (ch.outFifo.begin(), ch.outFifo.end(), 0.0f);
        std::fill (ch.outAccum.begin(), ch.outAccum.end(), 0.0f);
        std::fill (ch.lastPhase.begin(), ch.lastPhase.end(), 0.0f);
        std::fill (ch.sumPhase.begin(), ch.sumPhase.end(), 0.0f);
        ch.rover = 0;
    }
}

void PitchFormantEngine::computeCepstralEnvelope (ChannelState& ch)
{
    // logMag -> IFFT -> cepstrum. Build a real, even (mirrored) length-fftSize
    // sequence so the result (the cepstrum) comes out real.
    for (int k = 0; k < fftSize; ++k)
        ch.fftBuf[(size_t) k] = { 0.0f, 0.0f };

    for (int k = 0; k < numBins; ++k)
    {
        const float logMag = std::log (ch.anaMag[(size_t) k] + 1.0e-7f);
        ch.fftBuf[(size_t) k] = { logMag, 0.0f };
        if (k > 0 && k < numBins - 1)
            ch.fftBuf[(size_t) (fftSize - k)] = { logMag, 0.0f };
    }

    fft.perform (ch.fftBuf.data(), ch.fftBuf2.data(), true); // -> cepstrum

    // Lifter: keep only the low-quefrency region (+ its mirror at the top
    // of the buffer) which carries the formant envelope; zero the rest,
    // which is the harmonic/pitch detail we do NOT want in the envelope.
    for (int n = cepstrumCutoff; n < fftSize - cepstrumCutoff; ++n)
        ch.fftBuf2[(size_t) n] = { 0.0f, 0.0f };

    fft.perform (ch.fftBuf2.data(), ch.fftBuf.data(), false); // -> smoothed logMag

    for (int k = 0; k < numBins; ++k)
        ch.envelope[(size_t) k] = std::exp (ch.fftBuf[(size_t) k].real());
}

void PitchFormantEngine::processHop (ChannelState& ch, float pitchRatio, float formantRatio)
{
    const float freqPerBin = (float) sampleRate / (float) fftSize;
    const float expct = kTwoPi * (float) hopSize / (float) fftSize;

    // ---- windowing + analysis FFT --------------------------------------
    for (int k = 0; k < fftSize; ++k)
        ch.fftBuf[(size_t) k] = { ch.inFifo[(size_t) k] * analysisWindow[(size_t) k], 0.0f };

    fft.perform (ch.fftBuf.data(), ch.fftBuf2.data(), false);

    // ---- analysis: magnitude + true instantaneous frequency per bin ----
    for (int k = 0; k < numBins; ++k)
    {
        const float re = ch.fftBuf2[(size_t) k].real();
        const float im = ch.fftBuf2[(size_t) k].imag();

        // The 2x here mirrors Bernsee's convention: synthesis later only
        // fills the positive-frequency half of the spectrum and takes the
        // real part of the inverse transform (a single-sideband trick),
        // which halves amplitude - this doubling on the analysis side
        // cancels that out.
        const float magnitude = 2.0f * std::sqrt (re * re + im * im);
        const float phase = std::atan2 (im, re);

        float delta = phase - ch.lastPhase[(size_t) k];
        ch.lastPhase[(size_t) k] = phase;
        delta -= (float) k * expct;

        // Wrap to [-pi, pi] by subtracting the nearest multiple of 2*pi.
        int qpd = (int) (delta / kPi);
        qpd += (qpd >= 0) ? (qpd & 1) : -(qpd & 1);
        delta -= kPi * (float) qpd;

        delta = (float) oversamp * delta / kTwoPi;
        const float trueFreq = (float) k * freqPerBin + delta * freqPerBin;

        ch.anaMag[(size_t) k] = magnitude;
        ch.anaFreq[(size_t) k] = trueFreq;
    }

    // ---- cepstral split: envelope (formant) vs excitation (pitch) ------
    computeCepstralEnvelope (ch);
    for (int k = 0; k < numBins; ++k)
        ch.excitation[(size_t) k] = ch.anaMag[(size_t) k] / juce::jmax (ch.envelope[(size_t) k], 1.0e-6f);

    // ---- pitch shift: resample the excitation bin grid by pitchRatio ---
    std::fill (ch.synMag.begin(), ch.synMag.end(), 0.0f);
    std::fill (ch.synFreq.begin(), ch.synFreq.end(), 0.0f);

    for (int k = 0; k < numBins; ++k)
    {
        const int idx = (int) std::lround ((float) k * pitchRatio);
        if (idx >= 0 && idx < numBins)
        {
            ch.synMag[(size_t) idx] += ch.excitation[(size_t) k];
            ch.synFreq[(size_t) idx] = ch.anaFreq[(size_t) k] * pitchRatio;
        }
    }

    // ---- formant shift: warp the envelope's frequency axis, then -------
    // ---- recombine with the (already pitch-shifted) excitation ---------
    const float invFormant = 1.0f / juce::jmax (formantRatio, 0.01f);
    for (int k = 0; k < numBins; ++k)
    {
        const float srcPos = (float) k * invFormant;
        const int i0 = juce::jlimit (0, numBins - 1, (int) srcPos);
        const int i1 = juce::jlimit (0, numBins - 1, i0 + 1);
        const float frac = srcPos - (float) i0;
        const float shiftedEnv = ch.envelope[(size_t) i0] + frac * (ch.envelope[(size_t) i1] - ch.envelope[(size_t) i0]);

        ch.synMag[(size_t) k] *= shiftedEnv;
    }

    // ---- synthesis: accumulate phase from synthesis frequency, --------
    // ---- fill only the positive-frequency half (single sideband) -------
    for (int k = 0; k < numBins; ++k)
    {
        float freqDev = ch.synFreq[(size_t) k] - (float) k * freqPerBin;
        freqDev /= freqPerBin;
        float tmp = kTwoPi * freqDev / (float) oversamp;
        tmp += (float) k * expct;

        ch.sumPhase[(size_t) k] += tmp;
        const float phase = ch.sumPhase[(size_t) k];
        const float magnitude = ch.synMag[(size_t) k];

        ch.fftBuf2[(size_t) k] = { magnitude * std::cos (phase), magnitude * std::sin (phase) };
    }
    for (int k = numBins; k < fftSize; ++k)
        ch.fftBuf2[(size_t) k] = { 0.0f, 0.0f };

    fft.perform (ch.fftBuf2.data(), ch.fftBuf.data(), true);

    // ---- windowed overlap-add -------------------------------------------
    const float olaNorm = kSynthesisScale / ((float) (fftSize / 2) * (float) oversamp);
    for (int k = 0; k < fftSize; ++k)
    {
        const float w = analysisWindow[(size_t) k];
        ch.outAccum[(size_t) k] += 2.0f * w * ch.fftBuf[(size_t) k].real() * olaNorm;
    }

    for (int k = 0; k < hopSize; ++k)
        ch.outFifo[(size_t) k] = ch.outAccum[(size_t) k];

    std::copy (ch.outAccum.begin() + hopSize, ch.outAccum.end(), ch.outAccum.begin());
    std::fill (ch.outAccum.end() - hopSize, ch.outAccum.end(), 0.0f);

    std::copy (ch.inFifo.begin() + hopSize, ch.inFifo.end(), ch.inFifo.begin());
}

void PitchFormantEngine::process (juce::AudioBuffer<float>& buffer, float pitchRatio, float formantRatio)
{
    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin (buffer.getNumChannels(), (int) channels.size());

    for (int c = 0; c < numCh; ++c)
    {
        auto& ch = channels[(size_t) c];
        float* data = buffer.getWritePointer (c);

        for (int i = 0; i < numSamples; ++i)
        {
            const float outSample = ch.outFifo[(size_t) ch.rover];

            ch.inFifo[(size_t) (fftSize - hopSize + ch.rover)] = data[i];
            data[i] = outSample;

            ++ch.rover;
            if (ch.rover >= hopSize)
            {
                ch.rover = 0;
                processHop (ch, pitchRatio, formantRatio);
            }
        }
    }
}
