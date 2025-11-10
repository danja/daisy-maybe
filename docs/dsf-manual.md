# DSF Oscillator User Manual

Complete guide to operating the DSF Oscillator for kxmx_bluemchen.

## Table of Contents

1. [Overview](#overview)
2. [Front Panel Layout](#front-panel-layout)
3. [Basic Operation](#basic-operation)
4. [DSF Algorithms](#dsf-algorithms)
5. [Output Modes](#output-modes)
6. [MIDI Control](#midi-control)
7. [Through-Zero FM](#through-zero-fm)
8. [Patch Ideas](#patch-ideas)
9. [Display Guide](#display-guide)
10. [Tips & Tricks](#tips--tricks)

## Overview

The DSF Oscillator is a dual-oscillator Eurorack module implementing Discrete Summation Formula synthesis algorithms. It offers four synthesis algorithms, six output routing modes, MIDI control, through-zero frequency modulation, and extensive CV control.

### Key Specifications

- **Audio Rate**: 48 kHz
- **Frequency Range**: 55 Hz - 7040 Hz (7 octaves)
- **Harmonic Count**: 1-50 partials
- **CV Inputs**: 2x 0-5V (12-bit ADC)
- **Audio Inputs**: 2x Eurorack level
- **Audio Outputs**: 2x Eurorack level
- **MIDI**: DIN-5 input, channels 1 & 2
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
│     ○ POT 2 (Harmonics)         │
│                                 │
│     ┌───┐  Encoder               │
│     │ ⟲ │  (Algorithm/Mode)     │
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

### Getting Started

1. **Power on** the module (Eurorack power connected)
2. The display will show: "Classic DSF" algorithm
3. **Turn Knob 1** to set base frequency
4. **Turn Knob 2** to add harmonics
5. **Patch Output 1** to your mixer/destination

### Control Summary

| Control | Function | Range | Notes |
|---------|----------|-------|-------|
| Knob 1 | Base Frequency | 55 Hz - 7040 Hz | Exponential response |
| Knob 2 | Harmonics | 1-50 | Context-dependent in some modes |
| CV 1 | V/Oct Pitch | 5 octaves | Adds to base frequency |
| CV 2 | Alpha/FM Depth | 0.0-0.99 / 0-2x | Switches based on voltage |
| Audio In 1 | TZ-FM Input | Audio rate | Through-zero modulation |
| Audio In 2 | External Audio | Audio rate | For ring mod/processing |
| Encoder Rotate | Algorithm Select | 4 algorithms | Cycles through options |
| Encoder Short Press | Output Mode | 6 modes | Cycles through routing |
| Encoder Long Press | Display Toggle | 2 views | Hold >500ms |

### Display Views

**Normal View** (default):
- Line 1: Algorithm name + Output mode indicator (top right)
- Line 2: Frequency in Hz
- Line 3: Harmonics count (H:), Alpha value (A:) or FM depth (FM:)

**Extended View** (long press encoder):
- Line 1: "OUTPUT MODE:"
- Line 2: Current output mode name
- Line 3-4: MIDI status (if active) or FM depth

Press and hold the encoder for >500ms to toggle between views.

## DSF Algorithms

The module implements four Discrete Summation Formula algorithms, each with unique timbral characteristics.

### 1. Classic DSF (Moorer 1976)

**Character**: Bright, sawtooth-like waveforms with controlled harmonic rolloff

**Formula**: `y(t) = [sin(ωt) - α·sin(ωt - Nωt)] / [1 + α² - 2α·cos(Nωt)]`

**Parameters**:
- **N (Harmonics)**: Number of partials in the waveform
- **α (Alpha)**: Rolloff coefficient (0.0 = bright, 0.99 = dark)

**Sound**:
- Pure and cutting at low alpha values
- Warm and mellow at high alpha values
- Excellent for bass, leads, and pads

**Tips**:
- Alpha 0.0-0.3: Bright, buzzy tones (leads, plucks)
- Alpha 0.4-0.7: Balanced, musical sounds (pads, bass)
- Alpha 0.8-0.99: Soft, muted tones (sub bass, ambience)

### 2. Modified FM

**Character**: Metallic, bell-like timbres with inharmonic partials

**Implementation**: DSF with phase modulation where alpha controls modulation index

**Parameters**:
- **N (Harmonics)**: Complexity of the modulated spectrum
- **α (Alpha)**: FM modulation index (0.0 = subtle, 0.99 = extreme)

**Sound**:
- Bright, glassy tones with complex overtones
- Self-modulating character
- Excellent for bells, mallets, and FM-style sounds

**Tips**:
- Low harmonics (1-10) + high alpha: Clean FM bells
- High harmonics (30-50) + mid alpha: Dense, noisy textures
- Modulate alpha with CV for evolving timbres

### 3. Waveshape

**Character**: DSF with soft-clipping distortion

**Implementation**: Classic DSF followed by cubic waveshaping function

**Parameters**:
- **N (Harmonics)**: Base harmonic content
- **α (Alpha)**: Distortion amount (0.0 = clean, 0.99 = heavy)

**Sound**:
- Adds odd harmonics and warmth
- Tube-like saturation at moderate settings
- Aggressive distortion at high alpha

**Tips**:
- Alpha 0.0-0.2: Subtle warmth (analog character)
- Alpha 0.3-0.6: Moderate drive (overdriven sounds)
- Alpha 0.7-0.99: Heavy distortion (fuzz, grunge)

### 4. Complex DSF

**Character**: Multi-term summation with evolving, complex timbres

**Implementation**: Two DSF terms mixed together with harmonic shifting

**Parameters**:
- **N (Harmonics)**: Affects both fundamental and shifted series
- **α (Alpha)**: Controls interaction between the two terms

**Sound**:
- Rich, chorused character
- Subtle beating and movement
- Excellent for pads, drones, and evolving sounds

**Tips**:
- Creates natural movement without external modulation
- Works beautifully with slow LFO on alpha
- Great for ambient and experimental patches

### 5. Resonator Delay

**Character**: Dual independent resonator delays with pitch tracking

**Implementation**: Two delay lines process external audio inputs with V/Oct pitch control

**Parameters**:
- **POT 1**: Base delay time (1-250ms, exponential)
- **CV 1**: V/Oct delay time modulation (5 octave range)
- **CV 2**: Delay ratio for channel 2 (1:1 to 1:4 relative to channel 1)
- **MIDI Ch 1**: Pitch control for delay 1 (higher pitch = shorter delay)
- **MIDI Ch 2**: Pitch control for delay 2

**Signal Flow**:
- Audio In 1 → Delay Line 1 → Out 1
- Audio In 2 → Delay Line 2 → Out 2
- Independent delay times for each channel

**Sound**:
- Resonant metallic timbres when delays are short
- Karplus-Strong-style plucked sounds
- Stereo delay effects with ratio control
- Pitch-tracking delays for musical intervals

**Tips**:
- Short delays (1-20ms): Resonant, comb filter-like tones
- Medium delays (20-100ms): Slapback echo effects
- CV 2 at 50% creates a perfect octave relationship (1:2 ratio)
- CV 2 at 100% creates two-octave spread (1:4 ratio)
- Patch percussive sounds for physical modeling resonance
- Use MIDI to play delay times like a pitched instrument
- External feedback (patch outputs back to inputs) creates self-oscillation

**Control Mapping**:
- Harmonics control (POT 2) has no effect in this mode
- Alpha parameter unused
- Through-zero FM disabled
- Output modes have no effect (always independent channels)

## Output Modes

The module features six output routing configurations. Switch modes by **short pressing the encoder**.

### Mode 1: Mono Dual (M)

**Routing**: OUT 1 = OSC 1, OUT 2 = OSC 1 (identical)

**Use Cases**:
- Send same signal to two different processing chains
- Parallel effects routing
- Mult when you don't have one handy

**Controls**:
- Both pots and CVs affect the single oscillator
- MIDI Channel 1 controls pitch and velocity

### Mode 2: Stereo Detune (S) [Default]

**Routing**: OUT 1 = OSC 1, OUT 2 = OSC 2 (detuned +0.5%)

**Use Cases**:
- Instant stereo width
- Chorus-like thickness
- Rich pad sounds
- Stereo mixing

**Controls**:
- All controls affect both oscillators equally
- MIDI Channel 1 → OSC 1, Channel 2 → OSC 2 (additive)
- Detune is automatic, not adjustable

**Tips**:
- Perfect for creating wide, spacious sounds
- The 0.5% detune is musically subtle but effective
- Try with Complex DSF for maximum movement

### Mode 3: Dual Independent (D)

**Routing**: OUT 1 = OSC 1, OUT 2 = OSC 2 (independent)

**Use Cases**:
- Two-voice harmony
- Intervals (fifths, octaves)
- Complex chord structures
- Independent modulation

**Controls**:
- **Pot 1**: OSC 1 frequency
- **Pot 2**: OSC 2 harmonics (NOT OSC 1 harmonics!)
- **CV 1**: OSC 1 V/Oct pitch
- **CV 2**: OSC 2 pitch offset (0-2 octaves up)
- **MIDI Ch 1**: OSC 1 pitch/velocity
- **MIDI Ch 2**: OSC 2 pitch/velocity

**Tips**:
- Set CV 2 to 50% for perfect fifth interval
- Use MIDI channels for two-note melodies
- Different harmonic counts create interesting timbral contrast

### Mode 4: Main+Sub (B)

**Routing**: OUT 1 = OSC 1, OUT 2 = OSC 2 (-1 octave)

**Use Cases**:
- Bass weight and depth
- Techno/house bass lines
- Sub drops
- Foundation for main voice

**Controls**:
- Pot 2 controls harmonics for both
- OSC 2 automatically tuned one octave below OSC 1
- MIDI channels can independently control each

**Tips**:
- Essential for thick bass sounds
- Works great with Waveshape algorithm
- Try different harmonic counts for texture variation

### Mode 5: Main+Ring (R)

**Routing**: OUT 1 = OSC 1, OUT 2 = Ring Modulation

**Ring Mod Behavior**:
- **With cable in IN 2**: OUT 2 = OSC 1 × IN 2
- **Without IN 2**: OUT 2 = OSC 1 × OSC 2

**Internal Ring Mod**:
- OSC 2 tuned to perfect fifth (1.5x frequency) above OSC 1

**Use Cases**:
- Metallic timbres
- Bell-like sounds
- Frequency shifting
- Inharmonic spectra

**Tips**:
- Patch another oscillator into IN 2 for classic ring mod
- Use low harmonic counts for clearer ring mod tones
- Try Classic DSF algorithm for most controllable results

### Mode 6: Main+Process (P)

**Routing**: OUT 1 = OSC 1, OUT 2 = IN 2 × DSF envelope

**Behavior**:
- **With cable in IN 2**: External audio shaped by OSC 1's amplitude
- **Without IN 2**: Falls back to OSC 2

**Use Cases**:
- Add harmonic content to external audio
- Vocoder-like effects
- Rhythmic gating
- Waveshaping external sources

**Tips**:
- Patch drums into IN 2 for harmonic enhancement
- High harmonic counts create more texture
- The DSF amplitude provides natural envelope following

## MIDI Control

The module responds to MIDI Note On/Off messages on channels 1 and 2.

### MIDI Implementation

- **Channels**: 1 and 2 (polyphonic, one note per channel)
- **Messages**: Note On, Note Off
- **Note Range**: 0-127 (full MIDI range)
- **Velocity**: 0-127 (controls output gain)

### How MIDI Works

**Pitch Control**:
- MIDI note pitch is **added** to the manual frequency controls
- Formula: `Output Freq = (Pot 1 + CV 1 + MIDI Note) frequency`
- You can combine knob, CV, and MIDI pitch simultaneously

**Velocity Control**:
- MIDI velocity scales the output gain (0 = silent, 127 = full)
- When no MIDI note is active, gain = 1.0 (full volume)
- Allows for expressive dynamics from MIDI keyboards/sequencers

**Channel Routing**:
- **MIDI Channel 1** → Oscillator 1 (Output 1)
- **MIDI Channel 2** → Oscillator 2 (Output 2)

### MIDI Usage Examples

**Example 1: Single Voice with Velocity**
```
Setup:
- Output Mode: Mono Dual (M)
- MIDI Channel 1 from keyboard
- Pot 1 set to desired base pitch

Result:
- Keyboard plays melodic lines
- Velocity adds dynamics
- Pot 1 transposes everything
```

**Example 2: Two-Part Harmony**
```
Setup:
- Output Mode: Dual Independent (D)
- MIDI Channel 1 → melody track
- MIDI Channel 2 → harmony track
- CV 2 → adds interval offset to channel 2

Result:
- Two independent melodic lines
- OUT 1 plays melody
- OUT 2 plays harmony (transposed by CV 2)
```

**Example 3: MIDI Sequenced Bass**
```
Setup:
- Output Mode: Main+Sub (B)
- MIDI Channel 1 from sequencer
- High velocity notes for accents

Result:
- OUT 1 plays main bass note
- OUT 2 plays sub-octave
- Velocity creates dynamics
```

### Viewing MIDI Status

Long press the encoder to see the extended display:

```
OUTPUT MODE:
Stereo Detune
M1:N60 V100    <- Channel 1: Note 60 (C4), Velocity 100
M2:N67 V80     <- Channel 2: Note 67 (G4), Velocity 80
```

## Through-Zero FM

Through-zero frequency modulation allows the carrier frequency to pass through zero and become negative, causing phase inversion.

### Setup

1. **Patch** audio source (LFO, oscillator, etc.) to **Audio In 1**
2. **Set CV 2** > 0.1V to enable FM depth control
3. **Adjust CV 2** to control modulation intensity

### How It Works

- Audio In 1 modulates OSC 1's frequency
- When modulation drives frequency below zero, phase inverts
- Creates unique timbres impossible with standard FM
- FM Depth range: 0 to 2× base frequency

### TZ-FM Patch Ideas

**Slow Evolving Textures**:
```
Modulator: Slow LFO (0.1 - 2 Hz)
FM Depth: Medium (CV 2 = 50%)
Algorithm: Complex DSF
Result: Smooth, evolving pad sounds
```

**Metallic FM Tones**:
```
Modulator: Audio-rate oscillator
FM Depth: High (CV 2 = 80-100%)
Algorithm: Modified FM
Result: Classic FM synthesis timbres with through-zero character
```

**Percussive Sweeps**:
```
Modulator: Fast envelope (attack-release)
FM Depth: Full (CV 2 = 100%)
Algorithm: Classic DSF
Result: Frequency sweeps for kicks, toms, effects
```

**Randomized Pitch**:
```
Modulator: Sample & hold or noise
FM Depth: Low-medium (CV 2 = 20-40%)
Algorithm: Waveshape
Result: Glitchy, randomized pitch variation
```

## Patch Ideas

### Bread & Butter Sounds

**Thick Bass**:
```
Algorithm: Classic DSF
Output Mode: Main+Sub
Knob 1: Low frequency (100-200 Hz)
Knob 2: 15-25 harmonics
CV 2: Alpha = 0.5-0.7
```

**Stereo Pad**:
```
Algorithm: Complex DSF
Output Mode: Stereo Detune
Knob 1: Mid frequency (200-800 Hz)
Knob 2: 30-40 harmonics
CV 2: Slow LFO on alpha
```

**Cutting Lead**:
```
Algorithm: Classic DSF
Output Mode: Mono Dual
Knob 1: High frequency (800-2000 Hz)
Knob 2: 10-20 harmonics
CV 2: Alpha = 0.2-0.4
MIDI: Keyboard for melody
```

### Experimental Patches

**Self-Modulating Chaos**:
```
Algorithm: Modified FM
Output Mode: Main+Ring
Knob 2: 40-50 harmonics
CV 2: Alpha = 0.8-0.9
Patch: OUT 2 → Audio In 1 (feedback)
```

**Vocoder-Style Effect**:
```
Algorithm: Waveshape
Output Mode: Main+Process
Audio In 2: Voice or complex audio
Knob 2: 30-40 harmonics
CV 2: Alpha = 0.6
```

**Harmonic Bells**:
```
Algorithm: Modified FM
Output Mode: Main+Ring
Knob 1: Envelope into CV 1 (pitch)
Knob 2: 5-10 harmonics
CV 2: Alpha = 0.7
```

## Display Guide

### Normal Display Format

```
┌──────────────┐
│Classic DSF  S│  ← Algorithm name + Mode indicator
│F:440.0Hz     │  ← Frequency
│H:20   A:0.50 │  ← Harmonics / Alpha (or FM depth)
└──────────────┘
```

**Mode Indicators** (top right):
- `M` = Mono Dual
- `S` = Stereo Detune
- `D` = Dual Independent
- `B` = Main+Sub (Bass)
- `R` = Main+Ring
- `P` = Main+Process

### Extended Display Format

```
┌──────────────┐
│OUTPUT MODE:  │
│Stereo Detune │  ← Full mode name
│M1:N60 V100   │  ← MIDI Ch1 info (if active)
│M2:N67 V80    │  ← MIDI Ch2 info (if active)
└──────────────┘
```

Or if no MIDI:

```
┌──────────────┐
│OUTPUT MODE:  │
│Main+Ring     │
│FM:1.50       │  ← FM depth (if CV 2 > 0.1V)
│              │
└──────────────┘
```

## Tips & Tricks

### Sound Design Tips

1. **Start Simple**: Begin with Classic DSF, low harmonics, mid alpha
2. **Explore Harmonics**: The sweet spot is often 15-30 harmonics
3. **Alpha Movement**: Modulate alpha with slow LFO for organic evolution
4. **Stereo Width**: Stereo Detune mode is your friend for instant width
5. **Velocity Dynamics**: Use MIDI velocity for expressive performances

### Performance Tips

1. **Mode Switching**: Don't be afraid to switch output modes mid-performance
2. **Display Toggle**: Long press encoder to check MIDI/settings without changing sound
3. **CV Stacking**: CV 1 + MIDI pitch work together (great for modulation + melody)
4. **Quick Reference**: The single-letter mode indicator is visible at all times

### Workflow Tips

1. **Default to Stereo**: Stereo Detune is default for a reason - it sounds great
2. **Test in Mono First**: Design sounds in Mono Dual, then switch to Stereo
3. **Save Patch Notes**: The display shows all critical parameters
4. **MIDI as Offset**: Use MIDI to add intervals while CV 1 provides base pitch

### Troubleshooting Tips

1. **No Sound**: Check Knob 1 is not at minimum, check harmonics > 0
2. **Distortion**: Reduce harmonics or alpha, or lower input levels
3. **MIDI Not Working**: Verify channel 1 or 2, check extended display for status
4. **Unexpected FM**: Check CV 2 - if >0.1V it switches to FM mode
5. **Wrong Output Mode**: The mode indicator is always visible (top right)

### Creative Techniques

**Pseudo-Polyphony**:
Use Dual Independent mode with MIDI channels 1 & 2 for two-note chords.

**Harmonic Shifting**:
Modulate Knob 2 (harmonics) with slow CV for timbral evolution.

**Frequency Cascade**:
Patch OUT 2 (in Main+Sub) to a filter or VCA for separate sub-bass control.

**Ring Mod Exploration**:
In Main+Ring mode, try different external sources in Audio In 2 - drums work great!

**Through-Zero Drops**:
Use envelope into Audio In 1 with high FM depth for dramatic pitch drops.

---

## Appendix: Parameter Ranges

| Parameter | Minimum | Maximum | Resolution | Notes |
|-----------|---------|---------|------------|-------|
| Frequency | 55 Hz | 7040 Hz | Continuous | Exponential scaling |
| Harmonics | 1 | 50 | Integer | Steps of 1 |
| Alpha | 0.0 | 0.99 | Continuous | Rolloff coefficient |
| FM Depth | 0.0 | 2.0 | Continuous | Multiplier of base freq |
| CV 1 | 0V | 5V | 12-bit | V/Oct tracking |
| CV 2 | 0V | 5V | 12-bit | Dual function |
| MIDI Note | 0 | 127 | Integer | Full MIDI range |
| MIDI Velocity | 0 | 127 | Integer | 0 = silent, 127 = full |

## Appendix: Specifications

**DSP**:
- Platform: Electrosmith Daisy Seed (STM32H750, ARM Cortex-M7 @ 480MHz)
- Sample Rate: 48 kHz
- Bit Depth: 32-bit floating point internal, 24-bit output
- Algorithm: Band-limited Discrete Summation Formula

**Control**:
- ADC Resolution: 12-bit (4096 steps)
- Control Rate: ~1 kHz
- Display Update: ~30 Hz (33ms)
- MIDI: DIN-5, 31.25 kbaud

**Algorithms**:
- Classic DSF: Moorer (1976) formula
- Modified FM: DSF with phase modulation
- Waveshape: DSF + cubic soft-clipping
- Complex DSF: Dual-term summation

---

*For updates, issues, or contributions, visit the project repository.*
