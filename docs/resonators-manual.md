# Resonators User Manual

Guide to the dual-resonator delay firmware for kxmx_bluemchen.

## Overview

This firmware turns the module into a pair of tuned resonators with an input wavefolder/overdrive stage, filtered feed routing, selectable resonator models, clock-size/body scaling, and optional density controls. Each channel takes its own audio input and output, with pitch tracking set from Knob 1 and CV 1. The menu provides master mix and feed routing, distortion controls, model selection, resonator ratio, size, tap/density count, and filter shaping.

### Key Specifications

- **Audio Rate**: 48 kHz
- **Resonator Range**: ~10 Hz - 8 kHz (base), V/Oct tracking via CV1
- **Models**: tuned delay, modal bank, sympathetic strings, FM resonator
- **Delay Size / Body Size**: 1x-8x effective clock or body-size scaling
- **Output Taps / Density**: 1-5 normalized taps or model density control
- **CV Inputs**: 2x 0-5V
- **Audio Inputs**: 2x (one per resonator)
- **Audio Outputs**: 2x (one per resonator)
- **MIDI**: not used
- **Power**: Eurorack +12V/-12V

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
3. **Patch CV 1** for 1V/oct pitch control (5 octaves).
4. **Turn Knob 2 / CV 2** to set wavefolder depth.
5. **Rotate the encoder** to adjust the current menu item.
6. **Short press the encoder** to cycle through menu pages.

## Control Summary

| Control | Function | Range | Notes |
|---------|----------|-------|-------|
| Knob 1 | Base pitch | ~10 Hz - 8 kHz | Exponential mapping |
| CV 1 | V/Oct pitch | 5 octaves | Unipolar, scaled by calibration |
| Knob 2 | Wavefolder depth | 0.0 - 1.0 | Base fold depth |
| CV 2 | Wavefolder depth mod | 0.0 - 1.0 | Unipolar add to Knob 2 depth |
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
  - `Mode`: Resonator model. `0` Delay, `1` ModalBank, `2` SympString, `3` FMRes.
  - `Ratio`: Resonator Y ratio vs X delay time (0.25-4.0).
  - `Mix`: Resonator wet/dry mix at the output.
  - `Size`: Effective delay clock divisor (1-8). Higher values lengthen the delay lines and lower the resonant pitch for a larger-body response.
  - `Taps`: Number of output taps (1-5). Higher values add shorter prime-spaced taps for a reverb-like spread.
- **Distort**
  - `Fold`: Fold mix (dry ↔ folded).
  - `Drive`: Overdrive mix (dry ↔ driven).
  - `NFold`: Number of wavefolds (1-5).
- **Filter**
  - `Level`: Filter mix (dry ↔ filtered) on the feed paths.
  - `Freq`: Filter cutoff ratio (0.25-2.0) relative to each resonator pitch.
  - `Q`: Filter resonance (0.5–2.0).

## Calibration Mode (CAL)

Use CAL to fine-tune pitch tracking. While in CAL, the module outputs a 440 Hz sine tone on both outputs.

- **Knob 1**: Pitch scale (0.8–1.2)
- **Knob 2**: Pitch offset (±1 octave)

Settings are saved to flash automatically after about one second of inactivity.

## Resonator Models

`Mode` selects the resonator body while keeping the same pitch, wavefolder, feed, and mix controls.

- `0` **Delay**: original dual tuned delay-line resonators. `Ratio` tunes Y relative to X, `Size` changes effective delay clock, and `Taps` controls prime-spaced output taps.
- `1` **ModalBank**: Rings-inspired modal resonator bank. `Ratio` bends modal spacing from compressed to stretched, `Size` lowers the body and darkens upper modes, and `Taps` acts as modal density/pickup position.
- `2` **SympString**: Rings-inspired sympathetic string bank. `Ratio` morphs chord/spread relationships, `Size` changes string body size/decay, and `Taps` changes brightness and excitation density.
- `3` **FMRes**: Rings-inspired FM resonator voice. `Ratio` controls FM ratio, `Size` controls damping/envelope behavior, and `Taps` controls brightness/FM index.

## Feed Routing

The feed paths are filtered and summed before the wavefolder/overdrive stage. This keeps the feedback tone consistent even when distortion is pushed.

Tip: use higher `X-X` or `Y-Y` for strong single-resonator tones; add `X-Y`/`Y-X` for stereo interplay and coupled resonances.

## Filter

The feed paths run through a 2-pole lowpass filter before the distortion stage.

- Higher `Freq` keeps the feedback bright; lower values darken the tone.
- Higher `Q` emphasizes cutoff resonance; lower values are smoother.
- `Level` blends between dry feedback and filtered feedback.

## Size and Taps

`Size` changes the effective resonator clock divisor in Delay mode. At `1`, the resonator tracks the normal pitch range. Higher values multiply the delay time, producing a larger, lower resonant body while keeping the rest of the pitch-control path intact. In Rings-style modes, `Size` maps to body size or damping behavior.

`Taps` changes only the audible resonator output in Delay mode. The internal feedback path still uses the main tuned tap, while the output blends up to five shorter taps spaced at prime-like ratios. Tap gains are normalized by their total gain so adding taps creates density and reverb-like diffusion without a large level jump. In Rings-style modes, `Taps` becomes a density or brightness control.

## Mixes

Knob 2 sets the base wave depth, and CV 2 adds unipolar modulation on top of it. `Fold` and `Drive` set how much distortion is blended into the resonator input. This applies to every `Mode`, so the Rings-style models can be excited by clean, folded, or overdriven audio. `Mix` sets the resonator wet/dry output balance.

## Tips

- Patch noise or short percussive hits to excite the resonators.
- Use `Ratio` for musical intervals (0.5 = octave down, 2.0 = octave up).
- Increase `Size` for gong-like or body-resonance sounds; reduce it back to `1` for tighter pitched tracking.
- Increase `Taps` for a wider reverb-like tail without changing feedback stability.
- Cross-feed (`X-Y`/`Y-X`) can create stereo “chorus” effects when lightly applied.
- Use CAL before serious tracking work, especially if CV source is not perfectly scaled.
