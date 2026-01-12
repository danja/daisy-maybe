# DSF/Disyn Oscillator User Manual

Guide to operating the Disyn algorithm oscillator firmware for kxmx_bluemchen.

## Overview

This firmware replaces the original DSF algorithm set with the Disyn algorithm collection from the disyn-esp32 project. It provides 19 algorithms with consistent parameter mapping, two CV inputs tied to pitch and the primary algorithm parameter, and a simplified menu system for secondary parameters and output routing.

### Key Specifications

- **Audio Rate**: 48 kHz
- **Frequency Range**: 55 Hz - 7040 Hz (7 octaves)
- **Algorithms**: 19 Disyn oscillator algorithms
- **CV Inputs**: 2x 0-5V (12-bit ADC)
- **Audio Inputs**: Not used in this firmware
- **Audio Outputs**: 2x Eurorack level
- **MIDI**: DIN-5 input, channel 1
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
│     ○ POT 1 (Frequency)         │
│                                 │
│     ○ POT 2 (Param 1)           │
│                                 │
│     ┌───┐  Encoder               │
│     │ ⟲ │  (Menu)               │
│     └───┘                        │
│                                 │
│  CV 1  CV 2   IN 1   IN 2       │
│   ○     ○      ○      ○         │
│                                 │
│  OUT 1  OUT 2   MIDI IN         │
│   ○      ○       DIN            │
└─────────────────────────────────┘
```

## Basic Operation

1. **Turn Knob 1** to set base frequency.
2. **Patch CV 1** for V/Oct pitch modulation (0-5V = 5 octaves).
3. **Turn Knob 2** to set Param 1 (main algorithm parameter).
4. **Patch CV 2** to modulate Param 1.
5. **Rotate the encoder** to pick an algorithm (default page).
6. **Short press the encoder** to cycle through pages for Param 2, Param 3, and Output mode.

## Control Summary

| Control | Function | Range | Notes |
|---------|----------|-------|-------|
| Knob 1 | Base frequency | 55 Hz - 7040 Hz | Exponential response |
| Knob 2 | Param 1 | 0.0 - 1.0 | Algorithm dependent |
| CV 1 | V/Oct pitch | 5 octaves | Multiplies base frequency |
| CV 2 | Param 1 modulation | 0.0 - 1.0 | Adds to Knob 2 |
| Encoder rotate | Page value | Depends on page | See menu below |
| Encoder short press | Page select | ALG → P2 → P3 → OUT | Cycles pages |
| Encoder long press | Output overlay | On/Off | Shows output mode line |

## Menu System

The encoder controls a simple four-page menu:

- **ALG**: Select algorithm (encoder rotates through the 19 algorithms).
- **P2**: Adjust Param 2 (secondary algorithm parameter).
- **P3**: Adjust Param 3 (tertiary algorithm parameter).
- **OUT**: Select output mode.

Knob 1, Knob 2, and both CV inputs are always active regardless of page.

## Output Modes

- **Mono (M)**: Primary output on both channels.
- **Stereo (S)**: Primary on OUT 1, secondary on OUT 2.
- **Detune (D)**: Second oscillator detuned slightly on OUT 2.

## Algorithm List

Each algorithm exposes three parameters. Param 1 is on Knob 2 + CV 2. Param 2 and Param 3 are on the encoder pages.

**Primitive algorithms**
- **Dir Pulse**: Dirichlet pulse train with shaping.
  - P1 Harm (1-64), P2 Tilt (-3 to 15 dB), P3 Shape (0-1)
- **DSF S**: Single DSF component with sine blend.
  - P1 Decay (0-0.98), P2 Ratio (0.5-4), P3 Mix (0-1)
- **DSF D**: Dual DSF with positive/negative balance.
  - P1 Decay (0-0.96), P2 Ratio (0.5-4.5), P3 Balance (-1 to 1)
- **Tanh Sq**: Driven square wave.
  - P1 Drive (0.05-5), P2 Trim (0.2-1.2), P3 Bias (-0.4 to 0.4)
- **Tanh Saw**: Square-to-saw waveshaping.
  - P1 Drive (0.05-4.5), P2 Blend (0-1), P3 Edge (0.5-2.0)
- **PAF**: Phase-aligned formant texture.
  - P1 Form (0.5-6), P2 Bandwidth (50-3000 Hz), P3 Depth (0.2-1.0)
- **Mod FM**: Exponential FM-like modulation.
  - P1 Index (0.01-8), P2 Ratio (0.25-6), P3 Feedback (0-0.8)

**Combination algorithms**
- **C1 Hyb**: ModFM plus fixed formants.
  - P1 Index, P2 Unused, P3 Formant spacing (0.8-1.2)
- **C2 Cas**: DSF → asym FM → tanh.
  - P1 DSF decay, P2 Asymmetry (0.5-2), P3 Drive (0-5)
- **C3 Par**: Parallel ModFM bank + formants.
  - P1 Index, P2 Unused, P3 Mix (0-1)
- **C4 Fdb**: ModFM with feedback and drive.
  - P1 Index, P2 Feedback (0-0.95), P3 Drive (1-5)
- **C5 Mor**: DSF ↔ ModFM ↔ PAF morph.
  - P1 Morph, P2 Character, P3 Curve (0.5-2)
- **C6 Inh**: Inharmonic DSF into PAF.
  - P1 DSF decay, P2 PAF shift (5-50), P3 Mix (0-1)
- **C7 Flt**: Filter-like DSF/ModFM blend.
  - P1 Cutoff (0-1), P2 Resonance (0-1), P3 Mix (0-1)

**Novel algorithms**
- **N1 Mul**: Tanh → exp → ring modulation.
  - P1 Tanh drive (0.1-10), P2 Exp depth (0.1-1.5), P3 Ring ratio (0.5-5)
- **N2 Asy**: Frequency-dependent asym FM.
  - P1 Low R (0.5-1), P2 High R (1-2), P3 Index (0.2-1)
- **N3 XMod**: Cross-mod DSF/FM blend.
  - P1 Mod 1 (0-1), P2 Mod 2 (0-1), P3 Mix (0-1)
- **N4 Tay**: Taylor series approximation.
  - P1 Terms 1 (1-10), P2 Terms 2 (1-10), P3 Blend (0-1)
- **Traj**: Polygonal trajectory oscillator.
  - P1 Sides (3-12), P2 Angle (0-360), P3 Jitter (0-10 deg)

## MIDI Control

- **Channel 1 Note On**: Sets the base pitch and scales output level by velocity.
- **Channel 1 Note Off**: Releases velocity gain back to full.

MIDI pitch replaces Knob 1 base frequency while active, and CV 1 still applies as pitch modulation.

## Display Guide

- **Line 1**: Algorithm name with a leading `>` when the encoder is on ALG page. Output mode letter (M/S/D) on the right.
- **Line 2**: Frequency in Hz.
- **Line 3**: Param 1 label and value.
- **Line 4**: Param 2, Param 3, or Output depending on the current page. `>` marks the encoder target.

## Tips

- Use **P3 Mix/Balance** controls to create stereo interest in Stereo mode.
- Detune mode is subtle; increase pitch and drive parameters for obvious beating.
- For percussive sounds, try **Mod FM**, **C4 Fdb**, or **N1 Mul** with fast parameter modulation.
