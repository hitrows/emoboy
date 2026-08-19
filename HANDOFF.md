# HANDOFF — EmoBoy demo (autonomous session)

Working name **EmoBoy** (given directly by the user for this session — not
necessarily final branding; the brief's suggested placeholder name was not
used). Bundle ID `com.hitrows.emoboy`. AU + Standalone, Apple Silicon only,
this machine only. Built and validated on 2026-08-19.

This was an autonomous session per the brief in
`~/Downloads/logic-demo-brief-for-claude-code.md` — no check-ins, decisions
made and documented here for the user to review and correct.

## 2026-08-19 follow-up: modulation parked for "EmoBoy Nerd"

After listening, the user asked to focus on the plain core (Pitch/Formant/
Link/Mode/Drive/Mix) and park Mod1/Mod2/AM/PT + the routing matrix for a
future, more advanced variant — working name **"EmoBoy Nerd"**. Nothing was
deleted:

- `PluginEditor::buildModSection()` / `buildRoutingSection()` still exist,
  just no longer called from the constructor (commented out with the
  reasoning inline). Re-enabling the UI is those two lines back.
- All the underlying APVTS parameters (Mod1/Mod2/AM/PT knobs, all 32
  `route_*_on`/`route_*_depth` params), `ModMatrix`, `LFO`,
  `EnvelopeFollower`, and the routing logic in
  `PluginProcessor::updateModulationSources()`/`processBlock()` are
  untouched and still running every block - at zero audible effect, since
  no route is enabled by default. Nothing to port later; it's all live.
- The plugin window now sizes to just the main section instead of the full
  scrollable layout.

**Reminder for the user: bring this back into the UI when EmoBoy reaches
1.0** (or spin it into the separate "EmoBoy Nerd" product, per the working
name above) - whichever the user decides at that point.

### Follow-up: actually excluded from the build, not just hidden

The above (UI hidden, code still called) was the first pass. The user then
asked specifically not to carry the unused code into the actual build
output. Changed to a real compile-time gate:

- **`option(EMOBOY_NERD_FEATURES "..." OFF)`** in `CMakeLists.txt`, passed
  through as the `EMOBOY_NERD_FEATURES` compile definition.
- `Source/Parameters.h/.cpp`, `Source/PluginProcessor.h/.cpp`, and
  `Source/PluginEditor.h/.cpp` all wrap the Mod1/Mod2/AM/PT/routing-matrix
  code in `#if EMOBOY_NERD_FEATURES` / `#endif`. With the option OFF
  (default), none of it is compiled into the binary at all - not dead code
  sitting unused, genuinely absent from the object files.
- **`Source/dsp/Modulation.h`** (LFO/EnvelopeFollower/ModMatrix) is only
  `#include`d when the flag is on.
- To bring it back for real work: `cmake -B build -G Xcode
  -DEMOBOY_NERD_FEATURES=ON`, rebuild. Both configurations were built and
  `auval`-validated in this session - the gate itself isn't a guess.

**One regression this caught while doing it:** the pitch detector was
previously fed *only* from inside `updateModulationSources()` (the PT
source needed it, so it rode along). Naively gating that whole function out
would have silently broken Quantize/Robot mode too, since they also read
`pitchDetector`. Split into `feedPitchDetector()` (always compiled - the
mono downmix + `pitchDetector.pushBlock()`, called unconditionally near the
top of `processBlock`) and the Nerd-only `updateModulationSources()` (now
just reads the already-fed detector for PT, plus its own separate mono
downmix for the envelope follower). Worth remembering if this pattern comes
up again: check what a "modulation-only" function is quietly load-bearing
for before gating the whole thing out.

## Status: done, per the brief's own Definition of Done

- Builds via CMake + Xcode generator.
- `auval -v aufx Emob Htrw` → **AU VALIDATION SUCCEEDED**, no crashes across
  its sample-rate/buffer-size sweep (22050–192000 Hz, 64–4096 frames).
- Standalone launches and stays up (used it as a faster iterate loop instead
  of rescanning Logic every change).
- Pitch / Formant / Link / Mode (all three) / Drive / Mix are wired and
  numerically verified to affect the signal (see "Measurement" below).
- Mod1 (LFO) can be routed to Pitch and Formant via the routing grid and
  measurably alters the output.
- Everything beyond that in the brief's "final feature set" is also
  implemented (Mod2, AM, PT, the full 4×4 routing matrix, Robot mode with a
  note selector) — see "What's implemented" below for the one thing that
  was cut (live MIDI note tracking).

**The project folder is outside iCloud Drive and `~/Documents`**
(`/Users/hitrows/Developer/EmoBoy`), so the codesign/FinderInfo trap from
the "Not Sure" project does not apply here. Nothing to flag.

## The one deliberate cut: live MIDI note input

The brief listed "if a MIDI note is held, use it for Robot mode" as an
optional bonus, not blocking. It was tried first (`NEEDS_MIDI_INPUT TRUE`
in `juce_add_plugin`), but that changes the AU's registered component type
from **aufx** ("Effect") to **aumf** ("Music Effect") — Logic then wants a
MIDI-capable insert slot instead of a plain audio-track insert, which works
against the actual goal of this demo (drop it on a vocal track, listen).
Reverted: `NEEDS_MIDI_INPUT FALSE`, `acceptsMidi() = false`. Robot mode's
note source falls back to the explicit selector (C2–B3) unconditionally —
fully functional, just not MIDI-live. If a later build wants live MIDI, the
right fix is a runtime toggle the user can leave off by default rather than
forcing the whole plugin into the aumf category.

## What's implemented

Full parameter set from the brief's "final combined feature set", not just
the DoD minimum:

- **Pitch** ±12 st, **Formant** ±12 st (independent), **Link** (Formant
  becomes an *offset* from Pitch when enabled — see "Link semantics" below).
- **Mode**: Transpose / Quantize (chromatic hard-tune, YIN-based, Pitch knob
  adds transpose on top of the correction as specified) / Robot (flattens
  to one target note from a C2–B3 selector; MIDI variant cut, see above).
- **Drive** — fresh tanh waveshaper with asymmetric bias + matched
  pre/de-emphasis shelving, not ported from "Not Sure".
- **Mix** — dry path delay-compensated to the engine's exact analysis
  latency (2048 samples / ~46ms at 44.1kHz), reported via
  `setLatencySamples()` for host PDC.
- **Mod1/Mod2** (LFO: rate 0.06–30Hz, phase, level — no tempo sync, cut as
  the brief allowed), **AM** (envelope follower: attack/release/level),
  **PT** (pitch tracker: smooth/offset/level).
- **Routing matrix**: all 4 sources × 4 targets (Pitch/Formant/Mix/Drive),
  each an on/off + bipolar depth, built programmatically as
  `route_<source>_<target>_{on,depth}` parameter IDs.
- Plain JUCE UI (sliders/comboboxes/toggles in grouped sections, scrollable)
  — explicitly out of scope for real design per the brief.
- Fine pitch/cents control: **no separate fine-mode toggle** — the Pitch/
  Formant sliders just have 0.01-semitone resolution, which covers cents
  without extra UI. Flagging this as a simplification in case the user
  wanted an actual mode switch (coarse drag vs. fine drag), not just a
  precise range.
- Step sequencer (mentioned as a stretch item): **not built** — plain LFOs
  only, as the brief said was fine if time-constrained.
- Tempo sync for the LFOs: **not built**, same reasoning.

## Link semantics (a judgment call)

The brief says Link should make "Formant follow Pitch, preserving their
mutual offset — not just copying the value." Implemented as: when Link is
on, `effective formant = Formant-knob-value + Pitch-knob-value`, i.e. the
Formant knob becomes an *offset from* Pitch rather than an absolute value.
This needs no extra hidden state (no "offset captured at the moment Link
was engaged") and matches the wording directly. Worth the user's eyes: this
means turning Link on with Formant sitting at some absolute value will
suddenly reinterpret that value as an offset, which can be a visible jump
if Pitch is non-zero at that moment. An alternative (capture the delta only
at the instant Link toggles on) would avoid that jump but needs state and
wasn't obviously "more correct" from the brief's wording, so went with the
simpler version. Flagging for review, not confident this is the final
answer.

## Engine architecture: STFT phase vocoder + cepstral formant split

Per the brief: not TD-PSOLA (that's the target for the real product, is
next on the roadmap, not attempted here). `Source/dsp/PitchFormantEngine.*`
does:

1. **Analysis**: 2048-sample FFT, 512-sample hop (4× oversampling), Hann
   window. Bin-by-bin true instantaneous frequency via the classic
   Bernsee/DSPdimension phase-vocoder difference formula.
2. **Cepstral split**: `log(magnitude)` → IFFT → cepstrum → zero everything
   above a low-quefrency cutoff → FFT back → smooth envelope (formants).
   `excitation = magnitude / envelope` (the flattened harmonic buzz that
   carries pitch).
3. **Pitch shift**: the excitation spectrum's bin grid is resampled by the
   pitch ratio (magnitude + true-frequency accumulation, per-bin), same
   structure as smbPitchShift.
4. **Formant shift**: the envelope is warped along the frequency axis by
   the formant ratio (linear interpolation), independently of the pitch
   step.
5. **Recombine**: `pitch-shifted excitation × formant-shifted envelope`,
   accumulate synthesis phase, inverse FFT, windowed overlap-add.

This is what makes Pitch and Formant genuinely orthogonal instead of
coupled — the whole point of the brief's "combine AlterBoy + Vocal Bender"
framing.

### Measurement (the part that would have been silently wrong without it)

Built `tools/emoboy-render` (mirrors the "Not Sure" project's own
`notsure-render` workflow — an offline, JUCE-only-no-plugin-wrapper harness
that's much faster than opening Logic to check whether the DSP does what it
claims). Three things it caught that would NOT have been obvious by
reasoning about the code:

1. **Gain calibration.** JUCE's `dsp::FFT` does not scale the way
   Bernsee's original unnormalised-DFT-based formula assumes. First build
   was **~1365× too quiet** — reasoning about JUCE's FFT normalisation
   convention from memory would very likely have produced a wrong guess
   here rather than catching this; measuring did. `kSynthesisScale =
   1364.16f` in `PitchFormantEngine.cpp`, measured via the `[identity]`
   test (pitch=formant=1× should reproduce input RMS; converged to
   0.99999).
2. **Pitch accuracy**: verified directly (detect the output's actual f0 via
   the same YIN pitch detector used for Quantize/Robot, compare to
   requested ratio × input f0) rather than assumed from the ratio math.
   Within ~0.2% across the tested range (±5 to +12 semitones).
3. **Cepstral cutoff.** The "obvious" textbook heuristic
   (`sampleRate / 700`, → 63 quefrency samples at 44.1kHz) measured as a
   **near-total formant-shift failure** — a +7-semitone formant request
   moved the test signal's spectral peak by roughly 0 to ~2 semitones'
   worth, not 7. A sweep (`tools/render.cpp`, cutoff ∈ {12..90}) found a
   working value at **37 samples at 44.1kHz** (peak moved to 1211Hz against
   a computed target of 1198.6Hz — within ~1%). This is now the formula
   (`cepstrumCutoff = round(sampleRate × 37/44100)`), scaled proportionally
   with sample rate on the (untested at other rates) assumption that it's
   a frequency-resolution quantity.

**Known rough edge, not fixed in this session**: the cutoff's effect on
formant-shift accuracy is **not smooth** — neighbouring integer values (28,
31, 43, 46 in the sweep) measured as broken or badly undershooting, only a
narrow band around 34–40 worked well, on the synthetic test signal used
(110Hz fundamental + two formant bumps). This was measured on a stationary
synthetic tone, not real voice — real vocal material has continuously
varying pitch/formants and might behave more forgivingly, or might not.
**Worth listening critically to Formant specifically**, more than any other
control, before trusting it. If it sounds unstable or the shift amount
feels inconsistent across notes, this cutoff sensitivity is the first place
to look — likely fix is a proper minimum-phase / smoother envelope
construction rather than hard cepstral zeroing, which is a sharper filter
than the smoothness the technique wants.

### Latency

`getLatencySamples()` returns 2048 (the full analysis window) — reported to
the host via `setLatencySamples()` in `prepareToPlay`, and the dry path is
delayed by the same 2048 samples before mixing, so Mix < 100% should not
comb-filter. This was set to exactly match the FFT size rather than derived
from the exact internal FIFO bookkeeping (which is a slightly different,
smaller number in a classic Bernsee-style implementation) — simpler to
reason about and to guarantee dry/wet alignment is at least not habitually
short by a hop or two. Not needle-verified against the analysis via
cross-correlation the way gain and pitch were; if audible phasing shows up
at partial Mix in Logic, this is where to look first.

## Known artefacts / things to listen for

- **Formant accuracy/stability** — see cepstral cutoff note above. This is
  the highest-risk area in the whole engine.
- **Extreme pitch shifts** (well beyond the UI's ±12 semitone range isn't
  reachable from the knob alone, but modulation can push the *effective*
  ratio further — clamped at ±36 semitones total in
  `PluginProcessor::processBlock`) will show the bin-shifting technique's
  known weakness: sparse/holey spectra at large upward shifts, since it's
  literally moving bins rather than a proper resample+interpolate. Fine at
  demo-relevant ranges, listen for graininess if you push modulation depth
  hard.
- **CPU**: not measured or optimised. The pitch detector
  (`Source/dsp/PitchDetector.h`) re-runs a full O(n²) autocorrelation every
  processBlock call once its ring buffer fills, not just once per new
  window — cheap enough on Apple Silicon for a demo but is the obvious
  first thing to fix if this becomes a real product. `LimiterCore`-style
  perf discipline (Release-only measurement, etc.) from "Not Sure" was not
  applied here; this demo has not been profiled at all.
- **Quantize/Robot mode edge behaviour**: on unvoiced/silent input, Quantize
  falls back to plain Pitch-knob transpose (no correction), and Robot holds
  the last correction rather than snapping — both to avoid clicks on
  breaths, neither extensively tested against real singing, only reasoned
  through.
- **Stereo**: the engine runs one independent phase-vocoder channel per
  audio channel (no mid/side, no phase-locking across channels). A mono
  vocal on both channels should behave identically L/R; true stereo source
  material will get independent (uncorrelated) vocoder artefacts per
  channel, which may or may not be desirable — not evaluated either way.

## Next steps

1. **Listen.** This session could measure spectral peaks but not judge
   whether any of this sounds good. Start with Formant specifically per
   the cutoff note above.
2. **TD-PSOLA + LPC engine** — the brief's actual target architecture, not
   attempted this session. Near-zero latency (vs. this engine's ~46ms) is
   the main win; formant control via LPC coefficients directly rather than
   cepstral liftering would likely sidestep the cutoff-sensitivity issue
   entirely. This is a substantially larger undertaking than this session
   — different pitch-marking/epoch-detection machinery, not a drop-in
   swap for `PitchFormantEngine`.
3. If Formant turns out to need real fixing before it's worth pursuing
   TD-PSOLA at all: try a proper spectral envelope estimator (e.g. LPC via
   Levinson-Durbin, or true-envelope iterative cepstral smoothing) instead
   of one-shot hard cepstral liftering — the hard zeroing is a brick-wall
   filter in the quefrency domain, which is exactly the kind of sharp
   cutoff that tends to leak/ring.
4. Tempo-synced LFOs and the step sequencer, if the modulation section
   earns its keep in listening.
5. Live MIDI for Robot mode, as a runtime-optional toggle rather than a
   build-time `NEEDS_MIDI_INPUT` flag (see cut note above) — avoids forcing
   `aumf`.
6. Real UI/branding/name once the DSP direction is confirmed — everything
   visual in this build is a placeholder by design.

## Build

```sh
/opt/homebrew/bin/cmake -B build -G Xcode
/opt/homebrew/bin/cmake --build build --config Release --target EmoBoy_AU --target EmoBoy_Standalone
auval -v aufx Emob Htrw
```

Add `-DEMOBOY_NERD_FEATURES=ON` to the first line to bring back Mod1/Mod2/
AM/PT + the routing matrix (see "modulation parked for EmoBoy Nerd" above).
Reconfigure (the `cmake -B build` step) is required after flipping it -
building without reconfiguring keeps the old setting.

Numeric DSP check (faster than opening Logic for iteration):
```sh
/opt/homebrew/bin/cmake --build build --config Release --target emoboy-render
./build/tools/emoboy-render
```

JUCE is fetched via the same pinned tag (8.0.15) as "Not Sure", reusing that
project's already-cloned checkout (`FETCHCONTENT_SOURCE_DIR_JUCE` in
`CMakeLists.txt`) to skip a second multi-hundred-MB clone. If that project's
build directory ever gets cleaned, this will silently fall back to cloning
JUCE fresh — not a problem, just slower.

## Git

Local repo only, per the brief — nothing pushed, no remote configured.
