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

- **Master**
  - `DMIX`: Dry/wet mix between the direct input and the wavefolder/overdrive stage.
  - `RMIX`: Resonator wet/dry mix at the output.
  - `FXX`: Resonator X to X feed via filter.
  - `FYY`: Resonator Y to Y feed via filter.
  - `FXY`: Resonator X to Y feed via filter.
  - `FYX`: Resonator Y to X feed via filter.
- **Dist**
  - `FOLD`: Number of wavefolds (1–5).
  - `ODRV`: Overdrive amount (soft to hard clipping).
- **Res**
  - `RAT`: Resonator Y ratio vs X delay time (0.25–4.0).
  - `DMX`: Damping for resonator X feed filter.
  - `DMY`: Damping for resonator Y feed filter.

## Calibration Mode (CAL)

Use CAL to fine-tune pitch tracking. While in CAL, the module outputs a 440 Hz sine tone on both outputs.

- **Knob 1**: Pitch scale (0.8–1.2)
- **Knob 2**: Pitch offset (±1 octave)

Settings are saved to flash automatically after about one second of inactivity.

## Feed Routing

The feed paths are filtered and summed before the wavefolder/overdrive stage. This keeps the feedback tone consistent even when distortion is pushed.

Tip: use higher `FXX` for strong single-resonator tones; add `FXY`/`FYX` for stereo interplay and coupled resonances.

## Damping

`DMX` and `DMY` apply lowpass filters to the feed paths.

- Low damping = brighter, longer resonance
- High damping = darker, shorter resonance

## Mixes

`WMIX` sets how much wavefolder/overdrive is blended into the resonator input. `RMIX` sets the resonator wet/dry output balance.

## Tips

- Patch noise or short percussive hits to excite the resonators.
- Use `RAT` for musical intervals (0.5 = octave down, 2.0 = octave up).
- Cross-feed (`FXY`/`FYX`) can create stereo “chorus” effects when lightly applied.
- Use CAL before serious tracking work, especially if CV source is not perfectly scaled.
