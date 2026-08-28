# Resonators User Manual

Guide to the dual-resonator delay firmware for kxmx_bluemchen.

## Overview

This firmware turns the module into **two independent tuned resonators** — one per input/output pair, sharing a set of menu settings but nothing else. Pitch comes from Knob 1 and CV 1, with `Ofst` tuning the second channel against the first. Knob 2 and CV 2 set feedback, centre-off. In front of the resonators sits a wavefolder/overdrive stage, and in the feedback path a tuned filter.

The menu covers model selection, the two pitch controls, size and tap count, filter shaping, distortion, SD sample excitation, MIDI, and a page of live diagnostics.

Alongside the delay and string models there are five struck bodies — beam, marimba, drumhead, membrane and plate — which can be excited by the audio inputs, by a WAV sample loaded from the SD card, or by an internal mallet fired from MIDI.

### Key Specifications

- **Audio Rate**: 48 kHz
- **Resonator Range**: ~10 Hz - 8 kHz (base), V/Oct tracking via CV1
- **Models**: tuned delay, modal bank, sympathetic strings, FM resonator, beam, marimba, drumhead, membrane, plate
- **Delay Size / Body Size**: 1x-8x effective clock or body-size scaling
- **Output Taps / Density**: 1-5 normalized taps or model density control
- **CV Inputs**: 2x bipolar, -5V to +5V (0 V rests at mid-scale)
- **Audio Inputs**: 2x (one per resonator, no cross-feed; IN 1 doubles as a trigger input)
- **Audio Outputs**: 2x (one per resonator)
- **SD Card**: mono/stereo WAV excitation samples from `0:/resonators`
- **MIDI**: note pitch, velocity, and pitch bend
- **Power**: Eurorack +12V/-12V

### Flashing

This build targets the Seed's 8 MB QSPI flash rather than its 128 KB internal
flash, which it outgrew. Flash the Electrosmith bootloader to the module **once**:

```bash
make program-boot     # one time only, module in DFU mode
```

After that `make program-dfu` writes the firmware to QSPI exactly as before.

### Status

| Area | State |
|------|-------|
| Delay, and the five struck bodies | Working on hardware |
| Modal, Strng, FM | Working on hardware — retuned so they ring and decay rather than drone |
| Pitch, CV 1/CV 2, wavefolder | Working on hardware |
| SD sample excitation | Loads and plays; filename limit applies (see [Sample Excitation](#sample-excitation)) |
| Feedback on Knob 2/CV 2 | Active on **every** model; `Modal` maps it to damping. Exponentially tapered — a linear one was inaudible over most of its travel |
| Channel separation | Two independent instruments; only `Ratio` and the other menu settings are shared |
| **MIDI** | **Unverified — pending input-path checks. See [MIDI](#midi)** |

## Front Panel Layout

```
┌─────────────────────────────────┐
│         kxmx_bluemchen          │
│                                 │
│    ┌──────────────┐             │
│    │ OLED Display │             │
│    └──────────────┘             │
│                                 │
│     ○ POT 1 (Pitch)             │
│                                 │
│     ○ POT 2 (Feedback)          │
│                                 │
│     ┌───┐  Encoder               │
│     │ ⟲ │  (Menu)               │
│     └───┘                        │
│                                 │
│  CV 1  CV 2   IN 1   IN 2       │
│   ○     ○      ○      ○         │
│                                 │
│  OUT 1  OUT 2                   │
│   ○      ○                      │
└─────────────────────────────────┘
```

## Basic Operation

1. **Patch audio** to IN 1 and/or IN 2.
2. **Turn Knob 1** to set the base resonant pitch.
3. **Patch CV 1** for 1V/oct pitch control (±5 octaves).
4. **Turn Knob 2 / CV 2** to set feedback. It is centre-off: straight up is
   none, clockwise regenerates, anticlockwise regenerates inverted. The useful
   range is weighted towards the ends of the travel — see
   [Why the knob is tapered](#why-the-knob-is-tapered).
5. **Rotate the encoder** to adjust the current menu item.
6. **Short press the encoder** to cycle through menu pages.
7. **Optionally** load a WAV from the card on the **Sample** page and play the module from MIDI — see [Sample Excitation](#sample-excitation) and [MIDI](#midi).

## Control Summary

| Control | Function | Range | Notes |
|---------|----------|-------|-------|
| Knob 1 | Base pitch | ~10 Hz - 8 kHz | Exponential mapping. Drops out when MIDI `Ptch` is `Set` and a note is held |
| CV 1 | V/Oct pitch | ±5 octaves | **Bipolar**: 0 V is mid-scale and adds nothing. Scaled by calibration |
| IN 1 | Excitation, and trigger | — | Fires the sample on a rising edge when `Trig` is `In` or `Both` |
| MIDI note | Pitch and strike | — | See the MIDI page; `Ptch` chooses add vs. replace |
| MIDI velocity | Strike level | 0.0 - 1.0 | Scaled by `Velo` |
| Knob 2 | Feedback | -100% to +100% | **Centre-off**: straight up is none, clockwise in phase, anticlockwise inverted. Exponentially tapered |
| CV 2 | Feedback mod | ±100% | **Bipolar**: 0 V is mid-scale and adds nothing. Sums with Knob 2 |
| Encoder rotate | Menu value | Depends on item | See menu below |
| Encoder short press | Item select | Title → item list | Scrolls within a page |
| Encoder long press | Toggle CAL | CAL ↔ menu | CAL enters calibration tone |

## Menu Pages

The top line shows the current page title. When the title line is selected, rotating the encoder switches pages.

- **Resonate**
  - `Mode`: Resonator model, shown by name: `Delay`, `Modal`, `Strng`, `FM`, `Beam`, `Marim`, `Drumh`, `Membr`, `Plate`.
  - `Ratio`: Timbre control, read differently by each model (0.25-4.0) — chord spread on `Strng`, modal stretch on `Modal`, FM ratio on `FM`. On the struck bodies it stretches or compresses the partial series. It no longer moves the second channel's pitch; that is `Ofst`.
  - `Ofst`: Pitch of the second resonator relative to the first (0.25-4.0, default `1.00` = unison). `0.50` is an octave down, `2.00` an octave up.
  - `Mix`: Resonator wet/dry mix at the output.
  - `Size`: Effective delay clock divisor (1-8) on `Delay`; body size on every other model. Higher values lengthen the delay lines and lower the resonant pitch for a larger, longer-ringing body.
  - `Taps`: Number of output taps (1-5) on `Delay`, adding shorter prime-spaced taps for a reverb-like spread. On `Modal` it sets mode density and pickup spacing, on `Strng` brightness and string damping, on `FM` the modulation index, and on the struck bodies strike brightness and pickup position. It raises the models' own internal feedback, so it is the worst case the per-model feedback limits are measured against.
  - `Ring`: Decay time of the struck bodies (0-100%), scaling mode Q by about 4x end to end. Ignored by `Delay`, `Modal`, `Strng` and `FM` — on those, ring time comes from Knob 2 and `Taps`.
- **Distort**
  - `Fold`: Fold mix (dry ↔ folded). **Defaults to 0%** — the wavefolder is a colour, not the module's voice.
  - `Drive`: Overdrive mix (dry ↔ driven). Defaults to 0%.
  - `Dpth`: How hard the folder and overdrive are driven. This was Knob 2 before Knob 2 became the feedback control. With `Fold` and `Drive` both at 0% it does nothing.
  - `NFold`: Number of wavefolds (1-5).
- **Filter**
  - `Level`: Filter mix (dry ↔ filtered) on the feed paths.
  - `Freq`: Filter cutoff ratio (0.25-2.0) relative to each resonator pitch.
  - `Q`: Filter resonance (0.5–2.0).
- **Sample**
  - `File`: The selected WAV on the card, shown by name across the full row. With no card or no files, the row shows the card status instead.
  - `Load`: Rotate to load the selected file; the row shows the loader's status (`ready`, `loaded`, `trunc`, `no card`, `no wavs`, or a parse error). Rotating with no files rescans the card, so this doubles as a retry after a card swap.
  - `Levl`: How much of the sample is mixed into the excitation.
  - `Trig`: What fires the sample. `Off`, `MIDI` (note-on), `In` (a rising edge on IN 1), or `Both`.
- **MIDI**
  - `Chan`: `Omni`, or channels `1`-`16`.
  - `Ptch`: How a note relates to the knob/CV pitch. `Off` (notes trigger but never move the pitch), `Add` (note 60 is unity, the note offsets the knob/CV pitch), `Set` (note 69 is 440 Hz, the note owns the pitch and Knob 1 drops out).
  - `Velo`: How much velocity scales the strike. At `0` every note strikes at full level, for controllers with no velocity sensor.
  - `Malt`: Level of the internal mallet — a 3 ms noise burst fired on every note-on, so a note sounds with nothing patched and no card in the slot.
- **Diag** — read-only meters, written by the audio callback. Nothing here changes the sound; it exists to tell you *where* a problem is rather than that there is one.
  - `Feed`: Where Knob 2 and CV 2 have actually landed, signed, as a percentage. The knob has no detent, so this is the only way to find true centre.
  - `P2`: Knob 2 alone, 0-100. Should sweep the full range as you turn it.
  - `C2`: CV 2 alone, 0-100. An unpatched jack should sit near `50`.
  - `rP1`, `rP2`, `rC1`, `rC2`: the same four controls straight off the ADC, below the control layer. If the raw rows move and `P2`/`C2` do not, the fault is in the control config; if none of them move, nothing is reaching the ADC. Pot 1 and CV 1 are shown alongside as the known-good reference.
  - `zC2`: where CV 2 was measured to rest at power-up. CV 2 is read against this rather than an assumed mid-scale, so an input that does not sit where you expect cannot saturate the sum and take Knob 2 out with it. If the reading was moving at boot — something patched and active — this falls back to `50`.
  - `CPU`: Average audio-callback load, as a percentage.
  - `In`: Peak at IN 1/IN 2, measured at the jack before any processing.
  - `Out`: Peak at OUT 1/OUT 2, after the mix.
  - `Hz`: The pitch the resonators are actually being given. Reads `0` if it ever goes non-finite.
  - `Midi`: Count of MIDI messages received.
  - `MRx`: `1` while the MIDI UART is listening, `0` if the peripheral has stalled.
  - `Note`: Last MIDI note number received.

## Calibration Mode (CAL)

Use CAL to fine-tune pitch tracking. While in CAL, the module outputs a 440 Hz sine tone on both outputs.

- **Knob 1**: Pitch scale (0.8–1.2)
- **Knob 2**: Pitch offset (±1 octave)

Settings are saved to flash automatically after about one second of inactivity.

CAL and normal playback now read CV 1 on the same bipolar scale. They used to disagree — CAL read it bipolar while playback read it unipolar — so **calibration from a build before that fix does not carry over**. Redo CAL once after updating.

## Resonator Models

`Mode` selects the resonator body while keeping the same pitch, wavefolder, feed, and mix controls.

### Feedback models

- **Delay**: original dual tuned delay-line resonators. `Ofst` tunes the second relative to the first, `Size` changes effective delay clock, and `Taps` controls prime-spaced output taps.
- **Modal** (ModalBank): modal resonator bank. `Ratio` bends modal spacing from compressed to stretched, `Size` lowers the body and darkens upper modes, and `Taps` sets ring time (~0.3–0.8 s).
- **Strng** (SympString): sympathetic string bank. `Ratio` morphs chord/spread relationships, `Size` changes string body size, and `Taps` sets brightness and ring time (~0.3–0.9 s).
- **FM** (FMRes): FM resonator voice. `Ratio` controls FM ratio, `Size` controls damping/envelope behaviour, and `Taps` controls brightness/FM index. Rings ~0.6 s.

These four sit inside the feedback loop, but **not all of them take the same amount of it**. A plain delay line has no regeneration of its own, so the loop *is* its resonance and it wants the full amount. The others already ring internally, and the same feedback that makes `Delay` sing makes them lock into a continuous tone. Each is therefore scaled:

| Mode | Share of Knob 2 | Measured limit | Why |
|------|-----------------|----------------|-----|
| `Delay` | 100% | — | No internal feedback of its own |
| `Beam` | 45% | 0.70 | |
| `Plate` | 35% | 0.54 | |
| `Marim` | 30% | 0.48 | |
| `Drumh` | 28% | 0.42 | |
| `Membr` | 15% | 0.24 | The tightest of the bodies |
| `FM` | 13% | 0.20 | Self-oscillates just past it |
| `Strng` | 10% | 0.14 | Its strings regenerate, and `Taps` raises their feedback too |
| `Modal` | 0% | 0.00 | Sustains on the smallest loop that can be measured — Knob 2 damps instead, see below |

The scaling is applied for you — `Feed` on the **Diag** page still reads what you set. Turning Knob 2 up lengthens the ring; the range is just different per model.

These figures were measured rather than guessed, by sweeping feedback against ring time with the models running inside the real loop (`host_dsp/loop_fixture.cpp`). Testing a resonator open-loop is misleading here: `Modal` and `FM` both decayed perfectly well on their own and still droned in the module, because the loop was carrying them.

The **measured limit** column is where the ring stops decaying at the worst case on every axis — `Ring` at 1.00, which quadruples the bodies' mode Q, and `Taps` at 5, which raises the models' own internal feedback — with the values in use backed off about a third for margin. The struck bodies were all at 0% in an earlier build; that figure was measured when feedback was a stored setting that defaulted high, and re-measuring properly showed they take a great deal more than that.

### Knob 2 on `Modal`

`Modal` is the one model that can carry no feedback loop at all — at ringing Q it sustains on the smallest amount the sweep can resolve. Rather than leave the knob dead there, `Modal` reads it as **damping**: clockwise raises mode Q and the bank rings longer, anticlockwise damps it and the ring shortens. Measured end to end that is 0.14 s against 1.02 s, so it is the same musical gesture as feedback even though the mechanism is different.

The range is deliberately asymmetric. Upwards there is very little room before the bank self-oscillates on its own; downwards there is as much as you want, and damping is what makes `Modal` usable when it is too lively.

### Struck bodies

These five are modal resonators built from measured partial series. They are
struck rather than driven: they ring when something hits them — audio at the
inputs, a loaded sample, or the internal mallet. Knob 2 does work on all five,
over the per-body range in the table above.

Across all five, `Ring` sets the decay, `Size` makes the body bigger (lower and
longer-ringing), `Taps` sets strike brightness and pickup position, and `Ratio`
stretches or compresses the partial series — at `1.00` the body is physically
true, and away from it the partials bend and it stops sounding like itself.
`Ofst` detunes the second output against the first, independently of all that.

| Mode | Body | Partial series | Character |
|------|------|----------------|-----------|
| `Beam` | Free-free bar | (2n+1)²: 1, 2.76, 5.40, 8.93, … | Very inharmonic, very ringy. Struck metal bar. |
| `Marim` | Undercut bar | Tuned 1 : 4 : 10, then the bar law | Short, woody thock. The undercut tuning is what makes a marimba pitched where a plain bar is not. |
| `Drumh` | Timpani | 1 : 1.5 : 2 : 2.44 : 2.9 | Definite pitch. The kettle's air load pulls the membrane modes near-harmonic. |
| `Membr` | Ideal circular membrane | Bessel zeros: 1, 1.59, 2.14, 2.30, … | Unpitched and short. Toms and snares. |
| `Plate` | Simply-supported square plate | m²+n²: 1, 2.5, 4, 5, 6.5, … | Dense, metallic, slowest to decay. Degenerate mode pairs are split slightly, which is where the shimmer comes from. |

Partial ratios follow Fletcher & Rossing, *The Physics of Musical Instruments*.

## Sample Excitation

Put mono or stereo WAVs in `0:/resonators` on the card (the card root is used as
a fallback). PCM 8/16/24/32-bit and 32-bit float are all read; anything with more
than one channel is summed to mono. Files are loaded into SDRAM, up to about 43
seconds each — this is excitation material, so short hits work best.

**Filenames must be 63 characters or shorter.** Longer ones are skipped rather
than listed, because a truncated name lists fine and then fails to open, which
is a worse failure than not appearing; `Load` shows `name long` when that
happens. Note also that the `Load` row has only four columns left for its
status after the label, so the strings clip: `ready` reads as `read`, and
`open err` as `open`.

1. Open the **Sample** page and select `File`.
2. Rotate to pick a file.
3. Select `Load` and rotate. The row shows the loader's progress and result.
4. Set `Trig` to `MIDI`, `In`, or `Both`, and bring `Levl` up.

Loading is blocking and takes a moment for a long file, but audio keeps running
throughout: the player is detached before the store is rewritten, so the worst
case is silence from the sample path, never a burst of garbage.

The card is optional. With no card the module behaves exactly as it did before,
excited by its audio inputs and the internal mallet.

## MIDI

> **Status: unverified on hardware — pending input-path checks.**
>
> MIDI has not yet been observed working on a real module. On the test unit the
> UART reports healthy (`MRx` reads `1` on the Diag page) but no note data
> arrives.
>
> The firmware side has been checked against `dirac`, which does receive MIDI on
> this hardware: both take the libDaisy defaults (USART1, PB6/PB7, 31250 baud)
> through `Bluemchen::InitMidi`, both call `StartReceive` once and `Listen` from
> the main loop, and the parse and channel-filter paths are the same code. The
> one thing resonators had that dirac did not — a timed settle window — has been
> removed, and the fixed `128` the counter used to show is now understood and
> gone (see [Diagnosing MIDI](#diagnosing-midi)). So the remaining suspects are
> on the input path:
>
> 1. **The MIDI mezzanine.** The TRS jack is *not* on the main PCB. The
>    opto-isolator (U8, a TLP2362) is, but `MIDI_TIP`/`MIDI_RING` reach it
>    through a 2-pin header, **H3**, from a separate mezzanine board. If that
>    board is not fitted and connected there is no input path at all. Worth
>    checking first on a DIY build.
> 2. **TRS Type A vs Type B.** The schematic takes both tip and ring to the
>    opto, so a controller of the opposite polarity produces exactly this
>    signature: healthy UART, zero bytes.
>
> Use the **Diag** page to tell these apart — see
> [Diagnosing MIDI](#diagnosing-midi) below.

MIDI sets the resonator pitch, scales how hard it is struck, and fires samples.

- **Pitch**: `Ptch` on the MIDI page chooses whether a note offsets the knob/CV
  pitch (`Add`) or replaces it (`Set`). Pitch bend is ±2 semitones and applies in
  both modes. Note-off uses last-note priority, and the pitch persists after the
  final release so a body that is still ringing is not retuned out from under
  its own tail.
- **Velocity**: scales the sample trigger and the internal mallet. `Velo` sets
  how much — at `0`, every note strikes at full level.
- **Triggering**: with `Trig` set to `MIDI` or `Both`, each note-on plays the
  loaded sample once, transposed from note 60, up to four notes overlapping.

Because the bodies are passive, a note is not a voice: it sets the pitch and
fires a strike, then the body rings on its own for as long as `Ring` says.

### Diagnosing MIDI

At rest, `Midi` and `Note` should both read `0`. If they do not, stop here —
that is a firmware problem, not a wiring one.

`Midi` used to read a fixed non-zero number at power-up and never move again,
which looked like a stuck peripheral. It was a stale queue: reception starts
before the SD card is mounted and scanned, nothing drains the queue until the
main loop comes up, and the opto's power-up settling gets framed into a backlog
of bogus events that all land in the first pass. That backlog is now discarded
on the first pass, so `Midi` starts at a true zero and **any** increment is real
traffic. A timed settle window could not do this: if init ran past the window,
the backlog was already counted by the time it closed.

Play middle C and read the **Diag** page:

| `Midi` | `Note` | `MRx` | Meaning |
|--------|--------|-------|---------|
| climbs | `60` | `1` | Working. Anything still wrong is downstream of the parser |
| climbs | `0` | `1` | Bytes arriving but not parsing as notes — wrong channel, or framing |
| `0` | `0` | `1` | UART healthy, nothing arriving. Check H3/mezzanine and TRS type |
| — | — | `0` | The UART peripheral has stalled and is not recovering |

Set `Malt` up while testing so a note makes a sound with no sample loaded.

## Trigger Input

With `Trig` set to `In` or `Both`, a rising edge on IN 1 fires the sample. The
module has no gate jack, so IN 1 does double duty — the signal still passes
through to the resonator as excitation. Detection is a Schmitt trigger (0.15 up,
0.05 down) with a 5 ms lockout, so one pulse cannot spawn a burst. Audio inputs
are AC-coupled, so use trigger pulses rather than sustained gates.

## Feedback

Knob 2 and CV 2 set how much of each resonator's output returns to its own
input. The control is **centre-off**: straight up is no feedback at all,
clockwise regenerates in phase, and anticlockwise regenerates inverted — which
on a delay moves the comb's peaks onto the odd harmonics instead of the even
ones, a different tone rather than just less of the same one. Knob and CV sum,
so CV can reach either extreme from anywhere in the knob's travel. Read `Feed`
on the **Diag** page to find true centre.

The feed path is filtered before the wavefolder/overdrive stage, which keeps the
feedback tone consistent even when distortion is pushed.

### Why the knob is tapered

Ring time goes as 1/(1−g), so a feedback control with a linear taper is almost
entirely dead. Measured on `Delay`, three quarters of the travel sat at a 55 ms
tail and the whole audible range was crammed into the last few percent — the
knob worked the entire time, but there was nowhere to find it by turning it.

The control is mapped through `1−e^(−8k)` instead, normalised so full travel
still lands at exactly the per-model maximum in the table above. Measured across
the sweep the `Delay` tail now runs:

| Knob | 15% | 30% | 45% | 60% | 75% | 90% | 100% |
|------|-----|-----|-----|-----|-----|-----|------|
| Ring | 0.055 s | 0.155 s | 0.355 s | 0.705 s | 1.055 s | 1.205 s | 1.255 s |

Monotonic, roughly doubling per step, and never running away. `host_dsp/loop_fixture.cpp`
fails the build if two consecutive steps stop lengthening the ring.

Normalising the curve to exactly 1.0 matters for safety as well as feel: every
per-model limit was measured at full knob, so a taper that overshot would
invalidate all of them at once.

**There is no cross-channel feed.** X returns only to X and Y only to Y — see
[Two separate instruments](#two-separate-instruments).

## Two separate instruments

The two channels are two independent instruments, not one instrument heard from
two places. Each has its own excitation, its own resonator state, its own pitch,
and its own feedback path. Nothing crosses between them.

**Ratio is the exception.** It is the timbre control — chord voicing on `Strng`,
modal stretch on `Modal`, the C:M relationship on `FM`, partial stretch on the
struck bodies — and it applies to both channels, as do `Size`, `Taps`, `Ring`
and the distortion and filter settings. The two are the same *kind* of
instrument; what separates them is pitch, via `Ofst`, and whatever you patch to
each input.

Getting there meant unpicking mono summing in four places, any one of which put
IN 2's signal on OUT 1:

| Model | Was | Now |
|-------|-----|-----|
| `Modal` | One bank driven by `0.5·(X+Y)`, odd modes to OUT 1 and even to OUT 2 | Two banks, separately tuned and separately excited |
| `Strng` | One set of three strings driven by `0.5·(X+Y)`, panned across the outputs | Two string sets, same voicing, tuned apart by `Ofst` |
| `FM` | One carrier/modulator pair, one envelope from `0.5·(X+Y)` | A carrier/modulator pair and an envelope per channel |
| Struck bodies | One body driven by `0.5·(X+Y)`, two pickup positions | Two bodies, separately tuned; the two pickup positions are kept, one per channel, so the channels still differ in timbre |

The pickup and pan positions survived the change on purpose. Two identically
tuned bodies would give two identical outputs, and the point of the second
channel is that it is *not* a copy.

The cost is memory and a little arithmetic — coefficients are now built twice
per control tick instead of once — not any change to how a single channel
sounds.

## Filter

The feed paths run through a 2-pole lowpass filter before the distortion stage.

- Higher `Freq` keeps the feedback bright; lower values darken the tone.
- Higher `Q` emphasizes cutoff resonance; lower values are smoother.
- `Level` blends between dry feedback and filtered feedback.

## Size and Taps

`Size` changes the effective resonator clock divisor in Delay mode. At `1`, the resonator tracks the normal pitch range. Higher values multiply the delay time, producing a larger, lower resonant body while keeping the rest of the pitch-control path intact. In Rings-style modes, `Size` maps to body size or damping behavior.

`Taps` changes only the audible resonator output in Delay mode. The internal feedback path still uses the main tuned tap, while the output blends up to five shorter taps spaced at prime-like ratios. Tap gains are normalised by the root of their summed squares — the taps sit at incommensurate fractions of the delay, so what they return is uncorrelated, and normalising by the plain sum treated it as coherent and buried the level. Adding taps now creates density and reverb-like diffusion without a large level change. On the other models `Taps` becomes a density or brightness control: mode density and pickup spacing on `Modal`, brightness and damping on `Strng`, modulation index on `FM`. On the struck bodies it sets strike brightness — a hard mallet against a soft one — and moves the pickup position along the body, which is what keeps the two channels differing in timbre as well as pitch.

## Mixes

`Fold` and `Drive` set how much distortion is blended into the resonator input, and `Dpth` sets how hard it is driven. All three live on the **Distort** page; `Fold` and `Drive` both default to 0%, so the module is clean out of the box. This applies to every `Mode`, so the Rings-style models can be excited by clean, folded, or overdriven audio. `Mix` sets the resonator wet/dry output balance — wet and dry are level-matched, including on `Delay`.

## Tips

- Patch noise or short percussive hits to excite the resonators.
- Use `Ofst` for musical intervals between the two outputs (0.5 = octave down, 2.0 = octave up). `Ratio` is a timbre control, not a tuning one.
- Knob 2 straight up is silence in the feedback path, not "a bit of feedback". If a patch sounds dry and dead, that is where to start.
- Most of Knob 2's audible range sits in its upper half by design — see [Why the knob is tapered](#why-the-knob-is-tapered). Below about a third of travel the tail is short enough to read as none at all, and that is expected rather than a fault.
- Anticlockwise is not just "less". Inverted feedback puts the comb's peaks on the odd harmonics, which is a hollow, clarinet-like tone rather than a quieter version of the clockwise one.
- Increase `Size` for gong-like or body-resonance sounds; reduce it back to `1` for tighter pitched tracking.
- Increase `Taps` for a wider reverb-like tail. It does raise the models' own internal feedback, which is why the per-model feedback limits are all measured with `Taps` at 5 — the headroom quoted in the table is the worst case, not the typical one.
- Use CAL before serious tracking work, especially if CV source is not perfectly scaled. Redo it after updating from a build older than the bipolar CV 1 fix.
- If something sounds wrong, check the **Diag** page before changing settings — `In`, `Out` and `Hz` will usually say which end of the chain the problem is at.
- The struck bodies want transients, not drones. A short click, a mallet sample, or a MIDI note with `Malt` up will ring them; a sustained tone parked on a partial just drives the output into its clipper.
- `Marim` and `Membr` are the short ones; `Beam` and `Plate` are the long ones. Start there when choosing between them.
- `Ratio` at exactly `1.00` is the physically true body. Small moves either side detune the partials into something bell-like without losing the body's identity.
- On `Drumh` and `Membr`, low `Size` with a percussive sample gives usable drum voices straight out; raise `Size` for floor toms and gongs.
