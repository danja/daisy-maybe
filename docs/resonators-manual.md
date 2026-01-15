# Resonators User Manual

Guide to the dual-resonator delay firmware for kxmx_bluemchen.

## Overview

This firmware turns the module into a pair of tuned delay-line resonators. Each channel takes its own audio input and output, with delay lengths set so the resonant pitch tracks 1V/oct via CV 1. Cross-feedback, input injection position, damping, and wet/dry mix are available from the encoder menu.

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
| CV 1 | V/Oct pitch | 5 octaves | Bipolar around knob center |
| Knob 2 | Resonator 2 offset | -4 to +4 octaves | Bipolar offset around knob center |
| CV 2 | Resonator 2 offset mod | -4 to +4 octaves | Bipolar modulation |
| Encoder rotate | Menu value | Depends on page | See menu below |
| Encoder short press | Page select | CAL → FB → X12 → X21 → INP → DAMP → MIX | Cycles pages |

## Menu Pages

The top-right label shows the active page:

- **CAL**: Calibration mode (scale + offset).
- **FB**: Feedback amount for both delay lines.
- **X12**: Feedback from resonator 1 into resonator 2.
- **X21**: Feedback from resonator 2 into resonator 1.
- **INP**: Input injection position in each delay line.
- **DAMP**: Feedback damping (lowpass).
- **MIX**: Wet/dry mix per output.

## Calibration Mode (CAL)

Use CAL to fine-tune pitch tracking. While in CAL, the module outputs a 440 Hz sine tone on both outputs.

- **Knob 1**: Pitch scale (0.8–1.2)
- **Knob 2**: Pitch offset (±1 octave)

Settings are saved to flash automatically after about one second of inactivity.

## Feedback and Cross-Feedback

- **FB** sets the internal feedback gain for each resonator.
- **X12** and **X21** add cross-feedback between the two resonators for stereo interactions and coupled resonances.

Tip: use high FB with low cross values for stable resonant tones; push X12/X21 for chaotic, evolving textures.

## Input Position (INP)

INP controls where the input signal is injected inside each delay line, from the input tap to deeper inside the buffer.

- Low values = traditional input at the head (more predictable). At 0, input is mixed directly into the delay write.
- Higher values = darker, more complex resonances

## Damping (DAMP)

DAMP applies a lowpass filter to the feedback path.

- Low DAMP = brighter, longer resonance
- High DAMP = darker, shorter resonance

## Mix (MIX)

Sets dry/wet output balance for both channels.

- 0.0 = dry input
- 1.0 = full resonator output

## Tips

- Patch noise or short percussive hits to excite the resonators.
- Tune resonator 2 a fifth or octave above resonator 1 for stable intervals.
- Cross-feedback can create stereo “chorus” effects when lightly applied.
- Use CAL before serious tracking work, especially if CV source is not perfectly scaled.
