#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

// ---------------------------------------------------------------------------
// STFT phase-vocoder pitch/formant engine - the demo's deliberate shortcut
// vs. the eventual TD-PSOLA+LPC engine (see HANDOFF.md). Two things happen
// per analysis frame:
//
// 1. Pitch shifting: a phase vocoder (the classic Bernsee/DSPdimension
//    "smbPitchShift" structure - track each bin's true instantaneous
//    frequency via successive-frame phase difference, then resample the
//    bin grid by the pitch ratio) run on the EXCITATION spectrum only.
// 2. Formant shifting: cepstral deconvolution splits the magnitude
//    spectrum into a smooth "envelope" (low quefrency = formants) and a
//    flattened "excitation" (high quefrency = the harmonic buzz that
//    carries pitch). Only the envelope is warped along frequency for
//    Formant; only the excitation is resampled along frequency for Pitch.
//    Multiplying them back together after each has been shifted
//    independently is what makes Pitch and Formant genuinely orthogonal
//    instead of coupled the way a naive resample-and-repitch would be.
//
// Runs one instance of the analysis/synthesis machinery per channel.
// Everything here is control-rate-parameterised (pitch/formant ratio is
// read once per hop) - fine for a vocal effect, nothing here needs
// per-sample modulation resolution.
// ---------------------------------------------------------------------------
class PitchFormantEngine
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();

    // Exposed for tools/render.cpp's cutoff-tuning sweep only; the plugin
    // itself always uses the value prepare() derives from sample rate.
    void setCepstrumCutoffForTesting (int cutoff) noexcept { cepstrumCutoff = cutoff; }

    // Latency in samples introduced by the analysis window. Constant once
    // prepared - report it to the host via setLatencySamples().
    int getLatencySamples() const noexcept { return fftSize; }

    // pitchRatio/formantRatio: 1.0 = no shift, 2.0 = +12 semitones, 0.5 = -12.
    // Read once per processBlock call (control rate).
    void process (juce::AudioBuffer<float>& buffer, float pitchRatio, float formantRatio);

private:
    static constexpr int fftOrder = 11;              // 2^11 = 2048
    static constexpr int fftSize  = 1 << fftOrder;    // 2048
    static constexpr int oversamp = 4;                // 75% overlap
    static constexpr int hopSize  = fftSize / oversamp; // 512
    static constexpr int numBins  = fftSize / 2 + 1;

    struct ChannelState
    {
        std::vector<float> inFifo   = std::vector<float> (fftSize, 0.0f);
        std::vector<float> outFifo  = std::vector<float> (fftSize, 0.0f);
        std::vector<float> outAccum = std::vector<float> (fftSize, 0.0f);

        std::vector<float> lastPhase = std::vector<float> (numBins, 0.0f);
        std::vector<float> sumPhase  = std::vector<float> (numBins, 0.0f);

        std::vector<float> anaMag  = std::vector<float> (numBins, 0.0f);
        std::vector<float> anaFreq = std::vector<float> (numBins, 0.0f);

        std::vector<float> envelope   = std::vector<float> (numBins, 1.0f);
        std::vector<float> excitation = std::vector<float> (numBins, 0.0f);

        std::vector<float> synMag  = std::vector<float> (numBins, 0.0f);
        std::vector<float> synFreq = std::vector<float> (numBins, 0.0f);

        std::vector<std::complex<float>> fftBuf  = std::vector<std::complex<float>> (fftSize, { 0.0f, 0.0f });
        std::vector<std::complex<float>> fftBuf2 = std::vector<std::complex<float>> (fftSize, { 0.0f, 0.0f });

        int rover = 0; // Bernsee-style ring index into inFifo/outFifo
    };

    void processHop (ChannelState& ch, float pitchRatio, float formantRatio);
    void computeCepstralEnvelope (ChannelState& ch);

    std::vector<ChannelState> channels;
    std::vector<float> analysisWindow = std::vector<float> (fftSize, 0.0f);
    juce::dsp::FFT fft { fftOrder };
    double sampleRate = 44100.0;
    int cepstrumCutoff = 30;
};
