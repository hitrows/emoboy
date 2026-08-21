# HANDOFF — EmoBoy demo (autonomous session)

Working name **EmoBoy** (given directly by the user for this session — not
necessarily final branding; the brief's suggested placeholder name was not
used). Bundle ID `com.hitrows.emoboy`. AU + Standalone, Apple Silicon only,
this machine only. Built and validated on 2026-08-19.

This was an autonomous session per the brief in
`~/Downloads/logic-demo-brief-for-claude-code.md` — no check-ins, decisions
made and documented here for the user to review and correct.

## 2026-08-21: 0.1.15 - preset footswitch glow sizing, take two (right-edge bleed)

0.1.14's fix wasn't quite it. Tested live in Logic, the user reported:
button 4 now perfect, but 1/2/3 each visibly bled into their right-hand
neighbour's space (1 into 2, 2 into 3, 3 into 4) - not something either of
us could see in 0.1.14's per-button composite checks, since checking each
button in isolation can't reveal an overlap with its neighbour.

Cause: 0.1.14 measured the true button frame edges correctly, but then
padded a uniform ~10px on *both* sides - and the actual gaps between
buttons in the source art are only ~10-15px wide, so that padding alone
was enough to reach (or cross) into the next button over.

Fix: pulled back specifically the right edge of buttons 1/2/3 (button 4
has no right-hand neighbour and was already confirmed correct - left
untouched). Button 1 reverted to its exact original 0.1.13 bounds
(299-439) - it was never actually the problem, first flagged as correct
back in 0.1.13 and only started drifting when 0.1.14 touched it
unnecessarily. New bounds: 1 = 299-439 (w140, unchanged from 0.1.13),
2 = 438-553 (w115), 3 = 563-683 (w120), 4 = 688-823 (w135, unchanged).

**Checked differently this time**: composited all four buttons' glow onto
one continuous strip of `bg-clean.png` simultaneously (all "lit" at once,
even though only one is ever lit for real) instead of four separate
isolated crops - that's what actually shows adjacent overlap, which
isolated per-button checks structurally cannot.

Version bumped to 0.1.15. Pushed to `github.com/hitrows/emoboy`.

## 2026-08-21: 0.1.14 - preset footswitch glow sizing fixed (2/3/4 were oversized)

User caught it by eye: button 1's glow looked right, 2/3/4 didn't. Root
cause - 0.1.13's crop bounds came from the glow *sprite's own alpha
bounding box* in `"light transp.png"`, which over-captures because the
soft blur bleeds into the gaps between buttons. That happened to be a
small error for button 1 (~15px too wide) but a much bigger one for 2/3/4
(~45px too wide) - visible as an oversized glow box that didn't sit flush
against the actual button frame.

Re-measured properly this time: read the *actual button frame edges* in
`bg-clean.png` directly, via a fine (5px) pixel grid crop per button,
by eye - not another alpha-threshold pass, since that's exactly what
produced the wrong numbers the first time. Confirmed something genuinely
true about the source art in the process: button 1 really is ~10px wider
(125px) than buttons 2/3/4 (115px each) - not a measurement error to
"correct away", just how it was drawn. Padded a uniform ~10px around each
button's true edges and re-checked every one as a composite over
`bg-clean.png` before touching the code - all four now sit flush.

**Lesson for next time a glow crop looks slightly off**: measure the
*button frame* in the base art, not the glow sprite's own alpha bounds -
blur bleed makes the glow's bounding box an unreliable proxy for where the
button underneath actually is, especially when buttons sit close together.

Version bumped to 0.1.14. Pushed to `github.com/hitrows/emoboy`.

## 2026-08-21: 0.1.13 - 4 presets on the top-row footswitches, ROBOT button de-duplicated

Two fixes in one:

**ROBOT footswitch duplicate-functionality bug.** Once 0.1.11 added mode
button "3" (bottom row) as a real Robot-mode control, the pink ROBOT
footswitch (which still toggled Robot<->Transpose on click) was doing the
exact same job as a second, redundant control - both wired to the same
`mode` parameter, both visually lighting up together, but only one of them
needed a click handler. Per the user's own diagnosis ("косяк... дублирует
функционал"): `robotButton.onClick` removed entirely, plus
`setInterceptsMouseClicks (false, false)` so it doesn't even show a
pressable hover state for a click that would now do nothing. It's a pure
status indicator now, same pattern as HITROWS/PEAK - still lit whenever
Mode==Robot, driven by the same 30Hz timer as before.

**4 presets on the top-row "1"/"2"/"3"/"4" footswitches** (not the bottom
row - that's mode select). Named after the panel's own graffiti art
(VOID/CRY/LOST are printed on the pedal) plus one that exercises Robot
mode - not tuned against real vocal material by ear (can't), a reasonable
first pass per character:

| # | Name  | Pitch | Formant | Drive | Mix  | Mode      |
|---|-------|-------|---------|-------|------|-----------|
| 1 | Cry   | -3    | -3      | 15%   | 100% | Transpose |
| 2 | Void  | -12   | -9      | 65%   | 100% | Transpose |
| 3 | Lost  | +5    | +6      | 10%   | 80%  | Transpose |
| 4 | Robot | -     | +2      | 35%   | 100% | Robot (note: middle C) |

`EmoBoyEditor::applyPreset()` sets every relevant parameter explicitly
(not just the ones that "matter" for that preset) so each click is fully
deterministic regardless of prior state. Button "lit" state here isn't
polled from a parameter like every other backlight in this build -
presets aren't persisted state, just one-shot triggers - so `applyPreset()`
sets the clicked button lit and the other three unlit directly, once, on
click.

Full box-outline glow crops (top row's style, distinct from the bottom
row's under-glow-only look) measured off `bg-clean.png`/`"light
transp.png"` and cross-checked visually as composites before use, same
process as every other sprite. New `Resources/preset1glow.png`
through `preset4glow.png`.

**Verified via `tools/preview.cpp`**: since `applyPreset()` is private and
`createEditor()` only exposes the base `AudioProcessorEditor*`, replicated
the "Cry" and "Robot" presets' parameter values through the existing
public `setParam()` path and confirmed by eye that fader positions and
Mode-linked backlights (mode button "3" + the now-indicator-only ROBOT
footswitch, together) land where the presets table says they should.

Version bumped to 0.1.13. Pushed to `github.com/hitrows/emoboy`.

## 2026-08-20: 0.1.12 - PEAK lamp (input-level peak-hold indicator)

The small LED above "VOID" (labelled "PEAK" in the original, pre-label-
stripped reference photo) now actually does something. User first asked
whether this was worth building at all - recommended yes (it's literally
what the hardware art already implies), and specifically recommended a
**hard on/off snap** (fast attack, brief hold, instant off) over a smooth
level-proportional fade, since the user's own framing ("opacity прыгал с 0
до 100") described a jump, and that's also how real analog gear's PEAK
LEDs behave - matched the naming in the source art rather than building a
continuous VU meter. User initially deferred the whole feature ("забили
на лампочку пока"), then came back with a dedicated `pics/lamp.png` glow
sprite mid-session.

**DSP** (`PluginProcessor.h/.cpp`): peak measured on the *raw input*,
before the BYPASS early-return, so the lamp still works as a signal-
present monitor with BYPASS engaged. `abs(sample) >= kPeakThreshold`
(0.126 linear, ~-18 dBFS - a first guess tuned to react to normal singing
level, not just clip-level transients, not audited by ear) re-arms a
120ms hold counter each block; the counter counts down by
`buffer.getNumSamples()` per block and the lamp is lit exactly while it's
still positive - a real snap, not a decay curve. State lives in an
`std::atomic<bool> peakLedOn`, written on the audio thread, read by
`EmoBoyProcessor::isPeakLedOn()` from the editor's existing 30Hz timer -
same cross-thread pattern as everything else in this UI, no new
synchronisation primitive.

**Asset**: `Resources/lampglow.png`, cropped from `pics/lamp.png` (native
bounds 85,186 to 152,253, padded around the sprite's own alpha bounding
box) and composited over `bg-clean.png` to confirm placement before use.
`GlowToggleButton` reused again (non-interactive, like the HITROWS glow).

**Verified before calling it done** (`tools/preview.cpp`): `checkPeakLamp()`
confirms `isPeakLedOn()` is true after a loud (0.5 amplitude) block, false
after silence, and - importantly - **false at a quiet 0.01 amplitude**
too, confirming the threshold isn't so low it's effectively always on.
Also added a snapshot that pumps one loud block through the processor
before rendering, to visually confirm the lamp lights at the correct
position (not just that the boolean flips).

Version bumped to 0.1.12. Pushed to `github.com/hitrows/emoboy`.

## 2026-08-20: 0.1.11 - Mode select footswitches (1/2/3 = Transpose/Quantize/Robot)

Mode is now reachable from the UI via three real footswitches, not just
the ROBOT toggle. User pointed to `pics/123.png` (bg-clean with "1"/"2"/"3"
printed on the **bottom** row of blank buttons - explicitly not the
labelled "1/2/3/4" row above it, confirmed twice) as the reference for
which physical buttons these are.

**Shown a composite preview before writing any code**, per the user's
explicit ask: cropped each button's glow from `pics/"light transp.png"`
(bottom row - confirmed by eye these glow only along the bottom edge, no
full box outline like ROBOT/BYPASS have) and composited button 1's lit
look onto `bg-clean.png` to confirm the style before wiring anything up.
User confirmed with "да ты все правильно понял".

Bounds (native, all row y = 343-412): button 1 x 300-440, button 2 x
425-570, button 3 x 555-695 - measured off `123.png` directly rather than
assumed from the icon row above (the columns are aligned, but this row's
buttons aren't identically sized to the ones above).

`Source/PluginEditor.h/.cpp`: `modeButtons` is a `std::array<
GlowToggleButton, 3>`, index 0/1/2 wired to `Param::Mode::Transpose/
Quantize/Robot`. Each `onClick` sets the mode directly (not a toggle -
unlike the ROBOT footswitch, which still toggles Robot<->Transpose and
stays lit whenever Mode==Robot, so it and mode button "3" light up
together). `timerCallback()` (30Hz, same as every other backlight in this
build) keeps all three in sync with the current parameter value. New
`Resources/mode1glow.png` / `mode2glow.png` / `mode3glow.png`.

Verified via `tools/preview.cpp`: added a Quantize-mode snapshot,
confirmed against the existing default (Transpose) and Robot-mode
snapshots that exactly the right one of the three buttons lights up for
each mode, no bleed onto its neighbours.

Version bumped to 0.1.11. Pushed to `github.com/hitrows/emoboy`.

## 2026-08-20: 0.1.10 - Robot Note range actually moved to Logic's C2-B3 (not just relabelled)

Follow-up correction to 0.1.7/0.1.9. The user's real ask, stated
precisely on the second try: not "put the labels back to C2..B3" (which
0.1.9 already did, but on the *same* MIDI 36-59 range as before 0.1.7 -
just cosmetic, the actual pitch never moved) but **"actually make Robot
work on these notes"** - the true pitches Logic itself calls C2 through
B3.

Since Logic's convention is middle C = C3 = MIDI 60, Logic's C2 is
MIDI 48, not MIDI 36. `kRobotNoteBase` in `PluginProcessor.cpp` moved
from 36 to 48. Combined with the 0.1.9 label range (octave 2..3), index 0
now really is Logic's C2 (130.8 Hz), and - a happy consequence - index 12,
the knob's 12-o'clock rest position, lands exactly on **Logic's C3 =
MIDI 60 = middle C (261.6 Hz)**, about as clean a reference point as this
could have. Verified the three anchor frequencies directly (130.81 /
261.63 / 493.88 Hz for C2 / C3 / B3) against the standard MIDI-to-Hz
formula rather than trusting the constant change by eye.

Version bumped to 0.1.10. Pushed to `github.com/hitrows/emoboy`.

## 2026-08-20: 0.1.9 - HITROWS glow moved to its own asset file

User reorganised the source layers: reverted `pics/"light transp.png"`
back to footswitch-glows-only (its pre-0.1.8 state - no HITROWS in it
anymore), and split the wordmark glow into its own dedicated
`pics/hitrows.png`. Re-cropped `Resources/hitrowsglow.png` from that new
file (native bounds 130,130 to 440,240 - measured via alpha bounding box,
which came out tighter than the old crop from the shared sheet: 310x110
vs. 365x115, no "EMO BOY" bleed this time) and re-checked the composite
over `bg-clean.png` before use, same as every other glow sprite in this
project. `Resources/robotglow.png` and `bypassglow.png` are untouched -
the reverted shared sheet matches what they were already cropped from.

No logic changes - `GlowToggleButton`, the 30Hz timer sync, and
`! isBypassed` all carry over unchanged from 0.1.8. Re-ran the same
`tools/preview.cpp` bypassed/non-bypassed snapshots to confirm the new
crop still lands correctly with no bleed into the neighbouring "1" button.

Version bumped to 0.1.9. Pushed to `github.com/hitrows/emoboy`.

## 2026-08-20: 0.1.8 - HITROWS wordmark lights up when active

User updated `pics/"light transp.png"` again, adding a glow variant of the
HITROWS wordmark to the same sheet the footswitch glows live on. Asked for
it to light up whenever the plugin is doing something (i.e. not
bypassed), dark when BYPASS is engaged - a status indicator, not a
control.

Cropped the wordmark glow out (native bounds 130,130 to 495,245 - checked
visually as a composite over `bg-clean.png` first, same as the two
footswitch glows, to make sure the crop didn't bleed into the "1" button's
own glow sitting right next to it) into `Resources/hitrowsglow.png`.
Reused `GlowToggleButton` for the overlay machinery even though this
isn't a real button - `setInterceptsMouseClicks (false, false)` so it
never swallows a click meant for something underneath. Lit state driven
by the same 30Hz timer as the two footswitches: `hitrowsGlow.setLit
(! isBypassed)`.

Verified via `tools/preview.cpp`'s existing snapshot machinery: added a
`bypass=1` snapshot and visually confirmed the wordmark is lit in every
existing (non-bypassed) snapshot and dark in the new bypassed one.

Version bumped to 0.1.8. Pushed to `github.com/hitrows/emoboy`.

## 2026-08-20: 0.1.7 - Robot Note octave labels fixed to match Logic Pro

User asked to check the note-naming algorithm specifically against Logic
Pro's own octave convention. There are two real, incompatible octave-
numbering conventions in wide use for the same MIDI note numbers:

- **ASA / scientific pitch notation** (most synths, most other DAWs):
  middle C (MIDI 60) = **C4**.
- **Logic Pro's own convention**: middle C (MIDI 60) = **C3** - one
  octave lower than the above for the same physical pitch.

`Parameters.cpp`'s note-name generator was written against the first
convention (`for octave = 2 to 3`, matching MIDI 36-59 to "C2".."B3").
Checked the numbers directly: MIDI 36 = 65.4 Hz either way (the actual
sound was never wrong - the MIDI note number, not a note name, is what
drives pitch), but that frequency is "C2" under ASA notation and "C1" in
Logic. Exactly matches what the user reported by ear/eye while testing in
Logic: this build's "C3" sounded like Logic's own "C2", and this build's
"C2" was Logic's "C1" - a consistent one-octave-high labelling error
throughout the whole 24-entry list.

**Fix**: `for octave = 1 to 2` instead of `2 to 3` - only the display
strings change (now "C1".."B2"), matched to Logic's own numbering.
`kRobotNoteBase = 36` in `PluginProcessor.cpp` is untouched, since it maps
list-index to MIDI note number directly and was never wrong. Default index
is still 12, now labelled "C2" (was "C3") - same physical note, same
straight-up 12-o'clock resting position, just the correct name for it in
Logic.

Verified the generated list directly (`['C1','C#1',...,'B1','C2',...,
'B2']`, index 0/12/23 = C1/C2/B2) rather than trusting the loop bounds
change by eye. `auval` and the autogain/bypass numeric checks
(`tools/preview.cpp`) still pass unchanged, as expected - this was a
labelling-only fix, no DSP or MIDI-mapping code touched.

Version bumped to 0.1.7. Pushed to `github.com/hitrows/emoboy`.

## 2026-08-20: 0.1.6 - Robot Note knob reworked around C3, native dialogs, BYPASS footswitch

Three follow-ups in one go, same session as 0.1.5:

**1. Robot Note knob remapped around a centre reference note.** Original
0.1.5 mapping was linear index-0-to-23 across the full sweep. New ask: 12
o'clock = C3 (index 12) as a fixed reference, clockwise raises pitch a
semitone at a time to B3 at 5 o'clock (11 steps), counter-clockwise lowers
it to C2 at 7 o'clock (12 steps) - same ~300 degree sweep, just re-anchored
at the centre instead of one end. `angleForRobotNoteIndex()` in
`PluginEditor.cpp` replaced the old single linear formula with two
half-range ones. **Default parameter value changed from index 0 (C2) to
index 12 (C3)** (`Parameters.cpp`) - a fresh instance should rest with the
pointer straight up, not rotated to one side, which the old default would
have done under the new mapping. The two halves get very slightly
different angular steps (150 degrees over 11 steps vs. over 12) since C3
isn't exactly centred in a 24-note C2-B3 range - inherent to the user's
explicit endpoints, not a bug, and not perceptually significant (~9%
difference in step size between the halves).

Worth a note on process: the user's literal wording for this rework
("5 часов это C3... до B3 на 5 часов... до С2 на 5 часов") had the same
kind of typo as the 0.1.5 rotation-direction question - two different
endpoints both written as "5 часов". Given the entire session's
established convention (5 and 7 o'clock are always the two *different*
sweep ends), corrected it to 7 o'clock for the C2 end without stopping to
ask again - high-confidence pattern match, not a guess from nothing.

**2. Both popups reverted to plain/native JUCE dialogs, not custom-styled
panel overlays.** User's explicit ask, after seeing a Vocal Bender
reference screenshot for inspiration and then deciding against matching
its dark rounded-corner styling:
   - Robot Note knob double-click now opens a plain `juce::PopupMenu`
     (`EmoBoyEditor::showNotePicker()`) listing all 24 notes with a
     checkmark on the current one - no custom LookAndFeel colours applied.
   - Fader double-click text entry was rebuilt from the 0.1.4 custom
     `juce::Label`-based inline editor to a `juce::AlertWindow` with a text
     field and OK/Cancel buttons (`EmoBoyEditor::beginTextEntry()`) - a
     real modal dialog instead of a panel overlay. Guarded with a
     `Component::SafePointer<EmoBoyEditor>` in the modal callback since an
     `AlertWindow` is an independent top-level window, not a child
     component - unlike the old `Label` (destroyed automatically with its
     parent), it could otherwise outlive the editor and touch a dangling
     `fader` reference if the plugin window were closed while the dialog
     was still open.

**3. New BYPASS footswitch** (`Source/Parameters.h/.cpp`: `bypass` bool
param, wired via `EmoBoyProcessor::getBypassParameter()`, same pattern as
"Not Sure"). **Hard bypass** - `processBlock` returns immediately, before
any engine/Drive/auto-gain work, leaving the buffer completely untouched.
Does *not* attempt latency-compensated "smart" bypass, so toggling it
mid-playback can shift timing by the engine's ~46ms latency versus what
the host already compensated for - acceptable for this demo, worth
revisiting if bypass automation ever needs to be click-free.

Reused the Robot footswitch's glow-overlay component for this, renamed
from `RobotButton` to the more accurate `GlowToggleButton` since it now
backs two different buttons (only the glow sprite, position, and click
handler differ). New `Resources/bypassglow.png`, cropped from the same
`pics/"light transp.png"` sheet the Robot glow came from - this one
happens to glow **red** in the source art rather than pink, left as-is
since it reads naturally as an "off/bypassed" indicator color.

**Verified before calling it done** (`tools/preview.cpp`):
- Robot Note angle: snapshots at index 0 (C2), 12 (C3, left at its new
  default), and 23 (B3) land at 7, 12, and 5 o'clock respectively.
- Bypass: fed a 220Hz tone through `processBlock` with `bypass=1` and
  Pitch/Drive both pushed hard - `checkBypass()` confirms the output is
  **bit-identical** to the input (max sample difference exactly 0.0), not
  just "close".

Version bumped to 0.1.6. Pushed to `github.com/hitrows/emoboy`.

## 2026-08-20: 0.1.5 - Robot Note knob + Robot footswitch, first UI element beyond the 4 faders

User supplied 3 more layered assets in `pics/`: an updated `bg-clean.png`
(now includes a rotary knob graphic and an "Emo Boy" wordmark next to
"HITROWS"), `knob.png` (transparent, just a yellow guide circle marking
where the knob's face sits - measured via alpha bounding box: centre
(194,506), radius 28, cross-checked visually against the knob in
bg-clean.png), and `light transp.png` (every panel button's lit/backlit
look, pre-rendered as its own sprite with the glow already expanded ~3%/
softened ~30% - so no manual glow synthesis was needed, just cropping out
the Robot button's sprite).

**Shown before building anything**, per the user's explicit ask: two tick-
mark style options for the knob (bright branding pink vs. a muted tone
matching the fader caps' own inlaid stripe) and the Robot button's glow
composited onto the panel. User picked the muted tone and confirmed the
composited glow as-is.

**Two things worth being confirmed rather than assumed**, both resolved by
asking directly:
1. **Rotation direction.** The user's own words ("5 часов это C, 7 часов
   это B") describe the *opposite* of the standard convention (a normal
   pot's minimum/counterclockwise limit sits at ~7 o'clock, maximum at
   ~5 o'clock going the long way through 12) - taken literally, clockwise
   rotation would *lower* the note from B down to C. Asked rather than
   silently "fixing" it or blindly implementing the literal wording;
   confirmed the intent was actually the standard direction (clockwise =
   higher note, C at 7 o'clock/minimum, B at 5 o'clock/maximum) - which
   also happens to be JUCE's own default rotary-slider angle convention,
   so no inversion math was needed after all.
2. **Robot button behaviour**: confirmed as a toggle (Robot <-> Transpose),
   footswitch-style, matching the pedal metaphor. Quantize mode is still
   not reachable from this UI - unchanged from before, just noting it
   stays that way.

**Implementation** (`Source/PluginEditor.h/.cpp`):
- `RobotKnob : juce::Slider` (`RotaryHorizontalVerticalDrag` style, default
  LookAndFeel drawing suppressed same as `FaderOverlay`) draws only a
  tick line at an angle computed directly from the value - no
  `setRotaryParameters()` needed since the drawing is fully custom.
  Angle range -150deg to +150deg (measured clockwise from 12 o'clock),
  matching the confirmed 7-to-5-o'clock sweep through the top.
- `RobotButton : juce::Button` draws nothing itself when off (the unlit
  look is baked into the background photo); when `Mode == Robot`, draws
  the cropped glow sprite (`Resources/robotglow.png`) over its bounds.
  `onClick` toggles the `mode` parameter between `Robot` and `Transpose`
  directly.
- Kept in sync via a 30Hz `juce::Timer` polling `mode`'s raw parameter
  value, rather than `AudioProcessorValueTreeState`'s parameter-listener
  callback - that callback can fire from whatever thread changed the
  value (the audio thread, for host automation), and `repaint()` isn't
  safe to call off the message thread. Polling is simple and correct;
  30Hz is more than enough for a footswitch backlight.
- New `Resources/robotglow.png`, cropped from `pics/"light transp.png"`
  with padding around the measured alpha bounding box so the soft
  falloff isn't clipped, added to the `EmoBoyAssets` binary-data target.

**Verified via `tools/preview.cpp`** before calling it done (3 new
snapshot cases: `robotNote` = 0/11/23 with `Mode` forced to `Robot`) -
tick angle lands at 7 o'clock for index 0 (C), ~12 o'clock for index 11,
5 o'clock for index 23 (B), and the backlight is correctly off in the
`Mode != Robot` default snapshot and on in all three Robot-mode ones -
confirming the 30Hz timer sync works even in the offscreen test harness,
not just interactively.

Version bumped to 0.1.5. Pushed to `github.com/hitrows/emoboy`.

## 2026-08-20: 0.1.4 - user feedback: pitch step, Drive rescale, auto gain, type-in values

User's read after actually listening: **the Pitch/Formant engine beats Vocal
Bender** ("вокал бендер делает какой-то синтезированный голос, а наш супер
крут" - Vocal Bender sounds synthesized, ours doesn't). Strong validation of
the STFT phase-vocoder + cepstral-split approach from `PitchFormantEngine` -
worth remembering if a future session is ever tempted to second-guess that
architecture before TD-PSOLA lands. Formant and Drive both praised too.
Four concrete asks came with it:

1. **Pitch fader steps by 1 semitone, not 0.01.** `Parameters.cpp`:
   `NormalisableRange<float> (-12.0f, 12.0f, 1.0f)`. Formant is untouched -
   stays continuous, per "фейдер форманты... шикарен" (don't fix what
   isn't broken). JUCE's `SliderParameterAttachment` copies the
   parameter's `NormalisableRange` (interval included) onto the `Slider`
   automatically, and `Slider::setValue` snaps through
   `NormalisableRange::snapToLegalValue` before it ever reaches the
   parameter - confirmed by reading the JUCE source rather than assumed,
   since this also had to work correctly through the new double-click
   text entry (below), not just mouse drag.

2. **Drive rescaled**: what used to be the sound at 100% is now at 50%;
   100% pushes twice as far. `Drive::processSample`'s `amount` now runs
   0-2 instead of 0-1 (`PluginProcessor.cpp`: `driveAmount = (drivePercent
   / 100.0f) * 2.0f`). One deliberate exception inside `Drive.h`: the
   final dry/wet crossfade clamps its own copy of `amount` to a max of 1
   (`blendAmount`) rather than letting it extrapolate past the fully-wet
   shaped signal (2×output − x territory, which overshoots the tanh
   shaper's own bound instead of driving harder through it) - every other
   stage (pre-emphasis boost, drive gain, bias, de-emphasis blend) uses
   the full, unclamped 0-2 amount, which is what actually delivers "twice
   as much" character at the top of the range. At the new 50% mark this
   reproduces the pre-rescale 100% output exactly (every term evaluates
   identically at amount=1 either way) - not audited by ear (can't), but
   verified by inspection that the formulas are algebraically identical
   at that one point.

3. **New: automatic makeup gain**, always on, after Drive and the pitch/
   formant engine but before Mix - `Source/dsp/AutoGain.h`. Measures a
   ~300ms RMS envelope of both the wet (post-effects) and dry (already
   delay-compensated) signal and applies a smoothed correction gain
   (±15dB clamp, ~80ms smoothing) so turning up Drive/Pitch/Formant
   changes character, not perceived loudness - Mix then stays "how much
   character" instead of secretly also being a volume knob. No bypass
   parameter exists yet. **Verified numerically** (`tools/preview.cpp`,
   `checkAutoGain()`) rather than assumed: a 220Hz tone through
   pitch=0/+7/-7 combined with drive=0/25/50/100 all landed within
   **0.09 dB** of the input's RMS after settling past latency. Cheap
   insurance this session's "measure, don't assume" habit was worth
   keeping even under time pressure.

4. **Double-click any fader to type an exact value** (a mid-session
   addition to this same request). `FaderOverlay` gained an
   `onDoubleClick` callback; `EmoBoyEditor::beginTextEntry()` creates a
   `juce::Label` on demand, sized wider than the narrow fader hit-box and
   added as a child of the *editor*, not the fader itself (so it isn't
   clipped to the fader's ~24px-wide hit-box), pre-filled with the
   current value, auto-selected for immediate typing, and torn down via
   `Label::onEditorHide` once committed - the only text that ever appears
   on the panel, and only while someone is actively typing. **Not
   interactively tested** - no generic mouse/keyboard automation is
   available for an arbitrary macOS app in this session (only a browser
   pane and the iOS Simulator), so this was verified by reading the JUCE
   `Slider`/`Label` source for the exact mechanics (`showEditor()`
   requires an existing `valueBox`, which only exists when the text-box
   style isn't `NoTextBox` - worked around by using a separate, editor-
   owned `Label` instead of trying to repurpose the slider's own hidden
   text box) rather than by actually clicking it. Worth the user
   double-checking this one directly.

Version bumped to 0.1.4. Pushed to `github.com/hitrows/emoboy`.

## 2026-08-20: 0.1.3 - real fader-cap art, zero labels

User supplied proper layered assets in `pics/`: `bg-clean.png` (background
with every text label AND the fader caps themselves removed - just empty
grooves) and `faders.png` (one fader-cap sprite, copied 4× at its 4 resting
x-positions, transparent elsewhere). Replaces the 0.1.1/0.1.2 thin-pink-
line placeholder with the real cap graphic sliding along the track - what
the user asked for as "нормальные фейдеры".

**Before building, asked two questions** (per the user's own request to be
consulted from this point on):
1. The 4th (Mix) fader's track visually reads longer than the other
   three (starts higher) - give it its own longer travel, or keep all 4
   uniform? User said they'd send a reference image rather than answer
   directly.
2. With zero labels anywhere, is any hover-tooltip affordance wanted so
   someone other than the user could tell which fader is which? User: no,
   nothing - "если надо я тебе скажу" (they'll ask if they want it).

**The reference image** (`pics/lines.png`) settled Q1 definitively: three
horizontal guide lines (red/yellow/green = max/mid/min), spanning the full
width across all 4 fader columns in *one* guide, not per-column - i.e.
uniform travel for all four, not the taller-track theory. Measured via
alpha-channel search rather than eyeballed: red y≈504, yellow y≈634, green
y≈768. The yellow (mid) line landed within ~1px of the cap sprite's own
resting-position center (measured independently from `faders.png`'s alpha
bounding box, y 582-687 → center 634.5) - good cross-check that both
measurements are reading the same real thing.

**Implementation** (`Source/PluginEditor.h/.cpp`):
- `FaderOverlay` now takes a `const juce::Image&` cap reference and draws
  it (via `Graphics::drawImage` into a computed rect) instead of a line.
  Cap position = value mapped linearly between the red/green y's,
  centred both axes on the drawn rect.
- Each slider's hit-box is padded above/below the pure travel range by
  half the cap's height (53px native) - without this, the cap gets
  clipped by the component's own paint-clip region when a value sits at
  a true extreme (top of Mix at 100%, bottom of Drive at 0%, etc.), since
  `Component::paint` is clipped to `getLocalBounds()`.
- Fader x-centres re-measured from `faders.png`'s alpha bounding boxes
  (375/501/626/829) - supersedes the eyeballed 0.1.1 values, which were
  off by up to ~30px on the last two columns.
- The 0.1.2 "DRIVE"/"MIX" text-patch code (`kLabelFixes`, dark box + pink
  text drawn over the mismatched printed labels) is gone entirely - no
  longer needed, the new background has no labels to patch.
- New `Resources/fadercap.png` (cropped once from `faders.png`, 66x106,
  reused for all 4 sliders) added to the `EmoBoyAssets` binary-data
  target alongside the replaced `Resources/pedalbg.png`.

**A verification wrinkle worth remembering**: the first `emoboy-preview`
run after this change appeared to render the *old* labelled artwork with
the old thin-line indicators, even though `BinaryData.h`/`.cpp` and every
object file involved were freshly rebuilt (checked mtimes and the exact
embedded `pedalbg_pngSize` against the real file size - both matched the
new file). Re-running the exact same command with the old output files
deleted first produced the correct, current render. Never fully
root-caused (a `Read`-tool-side caching quirk on a just-rewritten path
seemed the most likely explanation, not a build problem) - but it's the
reason to re-verify with a byte-identical fresh file (delete-then-regenerate,
not just re-read) if a render ever looks implausibly stale again.

Version bumped to 0.1.3. Pushed to `github.com/hitrows/emoboy`.

## 2026-08-20: 0.1.2 - window scale, label fixes, GitHub

Three small follow-ups on the 0.1.1 skin, same session:

- **Window scaled to fit 1920x1080.** The photo's native 1074x976 didn't
  leave room for a DAW window next to it. Added `kUiScale = 0.6f` in
  `PluginEditor.cpp` - background draws scaled to the component bounds
  instead of at native size, fader hit-boxes scale by the same factor.
  Window now opens at ~644x586. Verified with a real screenshot (screen-
  recording permission was granted mid-session - see below).
- **Screen-recording permission granted mid-session.** Was missing
  earlier, which is why 0.1.1's fader alignment was verified through the
  offscreen `emoboy-preview` tool instead of a real screenshot. Now
  granted to `/Applications/Claude.app`. `emoboy-preview` is still kept
  around (useful for the next skin pass regardless).
- **"MIX BALANCE"/"REVERB" label mismatch patched at paint time.** The
  photo's own printed labels for the 3rd/4th faders don't match what
  they're wired to (Drive/Mix). Rather than edit the photo pixels, drew a
  dark backing box + the corrected word in the same pink as the indicator
  lines directly over each, in `EmoBoyEditor::paint()`
  (`kLabelFixes` array in `PluginEditor.cpp`). Coordinates for these two
  boxes were mis-measured on the first attempt - a crop that didn't
  extend far enough right silently returned an empty/black region for
  "REVERB" and the guessed coordinates put the "MIX" label past the
  photo's right edge entirely (1138px into a 1074px-wide image), where it
  got clipped by the window boundary. Re-measured with a properly-bounded
  grid crop before fixing - **worth remembering**: when a text/landmark
  search on a cropped image comes back empty, check the crop bounds
  before doubting the coordinates.
- **Version bumped to 0.1.2.**
- **Pushed to GitHub**: `github.com/hitrows/emoboy`, **private** (this
  wasn't explicitly specified - defaulted to private given how early/WIP
  this is and the pun-adjacent-trademark naming question already flagged
  for "Not Sure"; flip to public whenever the user wants).

## 2026-08-20: 0.1.1, first pedal-skin pass

User supplied a hardware-pedal mockup photo (`Resources/pedalbg.png`,
1074x976, saved from a pasted chat image at `~/Downloads/image.png`) and
asked for a version using it as the UI, with the 4 already-working
continuous parameters mapped onto 4 of its faders: 1st=Pitch, 2nd=Formant,
3rd=Drive, 4th=Mix. Note the photo's own printed labels are "Pitch /
Formant / **Mix Balance** / **Reverb**" - a placeholder-art naming
mismatch with the user's mapping, not a design decision; went with the
user's explicit words over the mockup's own text (position order matches
either way - it's the 3rd and 4th fader by position that are ambiguous in
name only). Confirmed with the user first (they invited questions before
any design work from this point on):

- **Indicator style**: a thin pink line at the current value (not a
  filled bar) - matches the pedal photo's own baked-in pink stripe on each
  cap.
- **Background**: the user's actual photo, not a placeholder color -
  "пока мы тестим только функции которые я написал на фейдерах, и в
  интерфейсе есть только они" (only the fader-mapped functions are being
  tested right now, nothing else belongs in the interface). So this build
  shows **only** the 4 faders - no Link/Mode/Robot Note/Bypass controls at
  all, even though those parameters still exist and work at their
  defaults under the hood (Mode=Transpose, Link=off).

**Implementation**: `PluginEditor` now draws the photo as a static
background (`juce::ImageCache` from `BinaryData::pedalbg_png`) and
overlays 4 `juce::Slider`s (`FaderOverlay`, `Source/PluginEditor.h/.cpp`)
whose `paint()` is fully overridden to draw nothing but a 3px pink bar at
the value position - no thumb, no track, no default LookAndFeel chrome, so
the photo's own fader artwork shows through and only the moving pink line
is drawn on top. Mouse drag still works normally (inherited from Slider).

**Fader pixel coordinates were measured, not eyeballed**: cropped the
photo with a pixel-labelled grid overlay (python/PIL) to find each fader's
x-centre and the y-range of its travel, rather than guessing from the
thumbnail. Then, since screen-recording permission wasn't available in
this session to literally screenshot the running app, built
`tools/preview.cpp` (`emoboy-preview` target) - an offscreen renderer that
instantiates the real `EmoBoyEditor`, sets parameters to specific test
values, and snapshots it to PNG via `Component::createComponentSnapshot`.
Rendered default/extremes/mid-range and visually checked the pink line
against the photo's own printed scale marks before calling it done:
extremes (±12st, 0/100%) land right on the "+OCT"/"-10"/"EFFECT"/"0"·"10"
labels; the default (0-value) position sits ~15-20px below the photo's own
baked-in decorative stripe, which is a quirk of where that stripe happens
to be drawn in the mockup art (not exactly at the travel's mathematical
centre), not a bug in the calculation - the extremes prove the math is
right. Good enough for a placeholder; will disappear once real per-state
fader-cap art replaces the single static photo.

**Not done in this pass** (all explicitly deferred to "normal design with
layers"): no separate cap-art / track-art layers (the photo is one flat
image), no styling for Link/Mode/Robot Note/Bypass (not shown at all),
window size is the photo's native 1074x976 pixels 1:1 (not resized to a
sane plugin-window size), no hover/drag visual feedback beyond the pink
line itself, `emoboy-preview` isn't cleaned up/removed (kept as a
reusable tool for calibrating future skin passes the same way).

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

UI/fader-calibration check (renders the real editor offscreen to PNG,
no window, no screen-recording permission needed):
```sh
/opt/homebrew/bin/cmake --build build --config Release --target emoboy-preview
cd /tmp && /path/to/build/tools/emoboy-preview
```

JUCE is fetched via the same pinned tag (8.0.15) as "Not Sure", reusing that
project's already-cloned checkout (`FETCHCONTENT_SOURCE_DIR_JUCE` in
`CMakeLists.txt`) to skip a second multi-hundred-MB clone. If that project's
build directory ever gets cleaned, this will silently fall back to cloning
JUCE fresh — not a problem, just slower.

## Git

Local repo only, per the brief — nothing pushed, no remote configured.
