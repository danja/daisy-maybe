# Resonators User Manual

Guide to the dual-resonator delay firmware for kxmx_bluemchen.

## Overview

This firmware turns the module into a pair of tuned delay-line resonators with an input wavefolder/overdrive stage and filtered feed routing. Each channel takes its own audio input and output, with delay lengths set so the resonant pitch tracks 1V/oct via CV 1. The menu provides master mix and feed routing, distortion controls, and per-resonator ratio and damping.

### Key Specifications

- **Audio Rate**: 48 kHz
- **Resonator Range**: ~10 Hz - 8 kHz (base), V/Oct tracking via CV1
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
│     ○ POT 2 (Offset)            │
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
4. **Turn Knob 2 / CV 2** to offset resonator 2 relative to resonator 1.
5. **Rotate the encoder** to adjust the current menu item.
6. **Short press the encoder** to cycle through menu pages.

## Control Summary

| Control | Function | Range | Notes |
|---------|----------|-------|-------|
| Knob 1 | Base pitch | ~10 Hz - 8 kHz | Exponential mapping |
| CV 1 | V/Oct pitch | 5 octaves | Unipolar, scaled by calibration |
| Knob 2 | Wavefolder depth | 0.0 - 1.0 | Base fold depth |
| CV 2 | Wavefolder depth mod | 0.0 - 1.0 | Adds to Knob 2 depth |
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
  - `RAT`: Resonator Y ratio vs X delay time (0.25–4.0).
  - `MIX`: Resonator wet/dry mix at the output.
- **Distort**
  - `FOLD`: Fold mix (dry ↔ folded).
  - `DRIV`: Overdrive mix (dry ↔ driven).
  - `NFLD`: Number of wavefolds (1–5).
- **Filter**
  - `MIX`: Filter mix (dry ↔ filtered) on the feed paths.
  - `FREQ`: Filter cutoff ratio (0.25–2.0) relative to each resonator pitch.
  - `Q`: Filter resonance (0.5–2.0).

## Calibration Mode (CAL)

Use CAL to fine-tune pitch tracking. While in CAL, the module outputs a 440 Hz sine tone on both outputs.

- **Knob 1**: Pitch scale (0.8–1.2)
- **Knob 2**: Pitch offset (±1 octave)

Settings are saved to flash automatically after about one second of inactivity.

## Feed Routing

The feed paths are filtered and summed before the wavefolder/overdrive stage. This keeps the feedback tone consistent even when distortion is pushed.

Tip: use higher `FXX` for strong single-resonator tones; add `FXY`/`FYX` for stereo interplay and coupled resonances.

## Filter

The feed paths run through a 2-pole lowpass filter before the distortion stage.

- Higher `FREQ` keeps the feedback bright; lower values darken the tone.
- Higher `Q` emphasizes cutoff resonance; lower values are smoother.
- `MIX` blends between dry feedback and filtered feedback.

## Mixes

`FOLD` and `DRIV` set how much distortion is blended into the resonator input. `MIX` sets the resonator wet/dry output balance.

## Tips

- Patch noise or short percussive hits to excite the resonators.
- Use `RAT` for musical intervals (0.5 = octave down, 2.0 = octave up).
- Cross-feed (`FXY`/`FYX`) can create stereo “chorus” effects when lightly applied.
- Use CAL before serious tracking work, especially if CV source is not perfectly scaled.
