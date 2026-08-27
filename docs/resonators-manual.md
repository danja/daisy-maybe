# Resonators User Manual

Guide to the dual-resonator delay firmware for kxmx_bluemchen.

## Overview

This firmware turns the module into a pair of tuned resonators with an input wavefolder/overdrive stage, filtered feed routing, selectable resonator models, clock-size/body scaling, and optional density controls. Each channel takes its own audio input and output, with pitch tracking set from Knob 1 and CV 1. The menu provides feed routing, distortion controls, model selection, resonator ratio, size, tap/density count, filter shaping, SD sample excitation, and MIDI.

Alongside the delay and string models there are five struck bodies — beam, marimba, drumhead, membrane and plate — which can be excited by the audio inputs, by a WAV sample loaded from the SD card, or by an internal mallet fired from MIDI.

### Key Specifications

- **Audio Rate**: 48 kHz
- **Resonator Range**: ~10 Hz - 8 kHz (base), V/Oct tracking via CV1
- **Models**: tuned delay, modal bank, sympathetic strings, FM resonator, beam, marimba, drumhead, membrane, plate
- **Delay Size / Body Size**: 1x-8x effective clock or body-size scaling
- **Output Taps / Density**: 1-5 normalized taps or model density control
- **CV Inputs**: 2x 0-5V
- **Audio Inputs**: 2x (one per resonator; IN 1 doubles as a trigger input)
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
│     ○ POT 2 (Fold)              │
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
4. **Turn Knob 2 / CV 2** to set wavefolder depth.
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
| Knob 2 | Wavefolder depth | 0.0 - 1.0 | Base fold depth |
| CV 2 | Wavefolder depth mod | ±1.0 | **Bipolar**: 0 V is mid-scale and adds nothing. Sums with Knob 2 |
| Encoder rotate | Menu value | Depends on item | See menu below |
| Encoder short press | Item select | Title → item list | Scrolls within a page |
| Encoder long press | Toggle CAL | CAL ↔ menu | CAL enters calibration tone |

## Menu Pages

The top line shows the current page title. When the title line is selected, rotating the encoder switches pages.

- **Wiring**
  - `X-X`: Resonator X to X feed via filter.
  - `Y-Y`: Resonator Y to Y feed via filter.
  - `X-Y`: Resonator X to Y feed via filter.
  - `Y-X`: Resonator Y to X feed via filter.
- **Resonate**
  - `Mode`: Resonator model, shown by name: `Delay`, `Modal`, `Strng`, `FM`, `Beam`, `Marim`, `Drumh`, `Membr`, `Plate`.
  - `Ratio`: Resonator Y ratio vs X delay time (0.25-4.0). On the struck bodies this stretches or compresses the partial series instead.
  - `Mix`: Resonator wet/dry mix at the output.
  - `Size`: Effective delay clock divisor (1-8). Higher values lengthen the delay lines and lower the resonant pitch for a larger-body response.
  - `Taps`: Number of output taps (1-5). Higher values add shorter prime-spaced taps for a reverb-like spread. On `Modal` and `Strng` it sets ring time instead (roughly 0.3 s to 0.9 s); on the struck bodies, strike brightness and pickup position.
  - `Ring`: Decay time of the struck bodies. Ignored by Delay, Modal, Strng and FM — on those, ring time comes from `Taps` and the feed amount.
- **Distort**
  - `Fold`: Fold mix (dry ↔ folded).
  - `Drive`: Overdrive mix (dry ↔ driven).
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

- **Delay**: original dual tuned delay-line resonators. `Ratio` tunes Y relative to X, `Size` changes effective delay clock, and `Taps` controls prime-spaced output taps.
- **Modal** (ModalBank): modal resonator bank. `Ratio` bends modal spacing from compressed to stretched, `Size` lowers the body and darkens upper modes, and `Taps` sets ring time (~0.3–0.8 s).
- **Strng** (SympString): sympathetic string bank. `Ratio` morphs chord/spread relationships, `Size` changes string body size, and `Taps` sets brightness and ring time (~0.3–0.9 s).
- **FM** (FMRes): FM resonator voice. `Ratio` controls FM ratio, `Size` controls damping/envelope behaviour, and `Taps` controls brightness/FM index. Rings ~0.6 s.

These four sit inside the feed matrix, but **not all of them take the same amount of it**. A plain delay line has no regeneration of its own, so the matrix *is* its resonance and it wants the full amount. The others already ring internally, and the same feed that makes `Delay` sing makes them lock into a continuous tone. Each is therefore scaled:

| Mode | Share of the `Wiring` values | Why |
|------|------------------------------|-----|
| `Delay` | 100% | No internal feedback |
| `Strng` | 22% | Its strings regenerate, and `Taps` raises their feedback too |
| `FM` | 15% | Self-oscillates above ~0.2 |
| `Modal` | 0% | At ringing Q even 0.04 sustains it indefinitely |
| Struck bodies | 0% | See below |

The scaling is applied for you — the `Wiring` numbers on screen stay as you set them. Turning `X-X` up still lengthens the ring on `Strng` and `FM`, just over a safe range.

These figures were measured rather than guessed, by sweeping feed against ring time with the models running inside the real feed loop (`host_dsp/loop_fixture.cpp`). Testing a resonator open-loop is misleading here: `Modal` and `FM` both decayed perfectly well on their own and still droned in the module, because the matrix was carrying them.

### Struck bodies

These five are modal resonators built from measured partial series. They are
excited **open-loop** — the `Wiring` page has no effect on them, because modes
this resonant oscillate at every partial at once inside any feedback path. They
ring when something hits them: audio at the inputs, a loaded sample, or the
internal mallet.

Across all five, `Ring` sets the decay, `Size` makes the body bigger (lower and
longer-ringing), `Taps` sets strike brightness and pickup position, and `Ratio`
stretches or compresses the partial series — at `1.00` the body is physically
true, and away from it the partials bend and it stops sounding like itself.

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
> arrives, which points at the input path rather than the firmware. Two things
> to check before suspecting the code:
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

Give the module a couple of seconds after power-up before reading these. MIDI
events are discarded for the first 1.5 s: the opto's output settles to idle-high
over some time after power-up, and until it does the UART frames noise into a
burst of bogus events. At rest, `Midi` and `Note` should both read `0`.

Then play middle C and read the **Diag** page:

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

## Feed Routing

The feed paths are filtered and summed before the wavefolder/overdrive stage. This keeps the feedback tone consistent even when distortion is pushed.

Tip: use higher `X-X` or `Y-Y` for strong single-resonator tones; add `X-Y`/`Y-X` for stereo interplay and coupled resonances.

`Wiring` has no effect on `Modal` or on the five struck bodies — all are excited
open-loop. See the table under [Feedback models](#feedback-models).

## Filter

The feed paths run through a 2-pole lowpass filter before the distortion stage.

- Higher `Freq` keeps the feedback bright; lower values darken the tone.
- Higher `Q` emphasizes cutoff resonance; lower values are smoother.
- `Level` blends between dry feedback and filtered feedback.

## Size and Taps

`Size` changes the effective resonator clock divisor in Delay mode. At `1`, the resonator tracks the normal pitch range. Higher values multiply the delay time, producing a larger, lower resonant body while keeping the rest of the pitch-control path intact. In Rings-style modes, `Size` maps to body size or damping behavior.

`Taps` changes only the audible resonator output in Delay mode. The internal feedback path still uses the main tuned tap, while the output blends up to five shorter taps spaced at prime-like ratios. Tap gains are normalized by their total gain so adding taps creates density and reverb-like diffusion without a large level jump. In Rings-style modes, `Taps` becomes a density or brightness control. On the struck bodies it sets strike brightness — a hard mallet against a soft one — and moves the pickup position along the body, which changes which partials the two outputs favour.

## Mixes

Knob 2 sets the base wave depth, and CV 2 adds unipolar modulation on top of it. `Fold` and `Drive` set how much distortion is blended into the resonator input. This applies to every `Mode`, so the Rings-style models can be excited by clean, folded, or overdriven audio. `Mix` sets the resonator wet/dry output balance.

## Tips

- Patch noise or short percussive hits to excite the resonators.
- Use `Ratio` for musical intervals (0.5 = octave down, 2.0 = octave up).
- Increase `Size` for gong-like or body-resonance sounds; reduce it back to `1` for tighter pitched tracking.
- Increase `Taps` for a wider reverb-like tail without changing feedback stability.
- Cross-feed (`X-Y`/`Y-X`) can create stereo “chorus” effects when lightly applied.
- Use CAL before serious tracking work, especially if CV source is not perfectly scaled. Redo it after updating from a build older than the bipolar CV 1 fix.
- If something sounds wrong, check the **Diag** page before changing settings — `In`, `Out` and `Hz` will usually say which end of the chain the problem is at.
- The struck bodies want transients, not drones. A short click, a mallet sample, or a MIDI note with `Malt` up will ring them; a sustained tone parked on a partial just drives the output into its clipper.
- `Marim` and `Membr` are the short ones; `Beam` and `Plate` are the long ones. Start there when choosing between them.
- `Ratio` at exactly `1.00` is the physically true body. Small moves either side detune the partials into something bell-like without losing the body's identity.
- On `Drumh` and `Membr`, low `Size` with a percussive sample gives usable drum voices straight out; raise `Size` for floor toms and gongs.
