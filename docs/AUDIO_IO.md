# Audio I/O Guide - Dual Outputs & Through-Zero FM

This enhanced version of the DSF oscillator makes full use of the bluemchen's stereo audio inputs and outputs.

## Audio Inputs

### Input 1: Through-Zero FM Modulator

**What is Through-Zero FM?**

Through-zero frequency modulation allows the carrier frequency to pass through zero and become negative, causing a phase inversion. This creates unique timbres impossible with standard FM.

**How to use:**
1. Patch any audio source into **IN 1**
2. Adjust **CV 2** (or knob if CV unplugged) to control FM depth
3. The input signal modulates the main oscillator's frequency
4. When modulation drives frequency negative, phase inverts (through-zero effect)

**Patch ideas:**
- LFO → smooth evolving timbres
- Audio rate oscillator → metallic FM sounds
- Envelope → percussive frequency sweeps
- Noise → randomized pitch variation
- Another oscillator → classic linear FM

**CV 2 controls FM depth:**
- 0V = no modulation
- 5V = maximum depth (~2x base frequency)

### Input 2: External Audio Processing

**What it does:**

Processes external audio through the DSF algorithms, or uses it as a ring modulation source.

**How to use:**

1. Patch audio source into **IN 2**
2. Select **Main+Ring** or **Main+Process** output mode (encoder press)
3. **Main+Ring**: Output 2 = Input 2 × Output 1 (classic ring mod)
4. **Main+Process**: Output 2 = Input 2 shaped by DSF envelope

**Patch ideas:**
- Drums → add harmonic content
- Voice → robotic vocoder effect
- Another synth → complex modulation
- External oscillator → ring modulation

## Audio Outputs

### Six Output Modes

Press the **encoder** (short press) to cycle through modes:

#### 1. Mono Dual
```
OUT 1: Main DSF oscillator
OUT 2: Identical copy
```
Use when you need two identical signals, or for parallel processing chains.

#### 2. Stereo Detune (Default)
```
OUT 1: Main oscillator
OUT 2: Slightly detuned (+0.5%)
```
Creates stereo width and chorus-like thickness. Perfect for:
- Pad sounds
- Rich leads
- Stereo mixing

The detune amount is subtle but effective for stereo imaging.

#### 3. Dual Independent
```
OUT 1: Oscillator 1 (controlled by main controls)
OUT 2: Oscillator 2 (independent pitch via CV 2)
```
Two completely independent DSF oscillators:
- **POT 1**: Frequency of oscillator 1
- **POT 2**: Harmonics of oscillator 2
- **CV 1**: V/Oct pitch for oscillator 1
- **CV 2**: Pitch offset for oscillator 2 (0-2 octaves up)

Perfect for:
- Intervals (fifths, octaves, etc.)
- Two-voice patches
- Complex chord structures
- Independent modulation

#### 4. Main + Sub
```
OUT 1: Main oscillator
OUT 2: Sub-octave (½ frequency)
```
Adds weight and depth with a sub-oscillator one octave below:
- Bass emphasis
- Thick techno bass
- Sub drops
- Foundation for main voice

Both oscillators share harmonic and alpha settings.

#### 5. Main + Ring
```
OUT 1: Main oscillator
OUT 2: Ring modulation (Main × Modulator)
```
Ring modulation creates metallic, bell-like timbres:

**Without external input (IN 2 unplugged):**
- Internal ring mod between main and second oscillator
- Second osc at perfect fifth (1.5× frequency)

**With external input (IN 2 patched):**
- Ring mod between main oscillator and external audio
- Creates extreme timbral shifts

Ring modulation produces sum and difference frequencies:
```
Output = (f1 + f2) and (f1 - f2)
```

#### 6. Main + Process
```
OUT 1: Main oscillator (unaffected)
OUT 2: External audio shaped by DSF characteristics
```

**Without external input:**
- OUT 2 = second DSF oscillator (backup mode)

**With external input (IN 2 patched):**
- OUT 2 = external audio × DSF amplitude envelope
- Creates dynamic processing that follows the DSF's harmonic evolution

Perfect for:
- Dynamic filtering effects
- Envelope following
- Harmonic enhancement
- Parallel processing with DSF character

## Display Information

### Normal View
```
Classic DSF         [M]
F:440.0Hz
H:20          FM:0.50
```
- Line 1: Algorithm name + mode indicator (M/S/D/B/R/P)
- Line 2: Frequency
- Line 3: Harmonics + Alpha or FM depth

**Mode indicators:**
- M = Mono Dual
- S = Stereo Detune
- D = Dual Independent
- B = Bass (Main+Sub)
- R = Ring modulation
- P = Process

### Extended View (Encoder long-press)
```
OUTPUT MODE:
Stereo Detune
FM:0.75
```
Shows current output mode and FM depth clearly.

Press and hold encoder >500ms to toggle views.

## Control Summary

| Control | Function | Range/Notes |
|---------|----------|-------------|
| **Pot 1** | Base Frequency | 55Hz - 7kHz (7 octaves) |
| **Pot 2** | Harmonics | 1-50 (or OSC2 harmonics in Dual mode) |
| **CV 1** | V/Oct Pitch | 5 octave range, adds to base freq |
| **CV 2** | FM Depth / Alpha | Auto-switches based on patch |
| **Encoder Rotate** | Algorithm | 4 algorithms: Classic, ModFM, Waveshape, Complex |
| **Encoder Press** | Output Mode | Cycles through 6 modes |
| **Encoder Hold** | Toggle View | Normal ↔ Extended display |
| **IN 1** | TZ-FM Modulator | Audio rate modulation with through-zero |
| **IN 2** | External Audio | Ring mod or process source |
| **OUT 1** | Main Output | Always main DSF oscillator |
| **OUT 2** | Secondary Output | Depends on output mode |

## Patching Examples

### Example 1: Stereo Bass with Movement
```
Patch:
- Nothing in inputs (self-contained)
- OUT 1 → Left channel
- OUT 2 → Right channel
- Mode: Stereo Detune

Settings:
- Low frequency (POT 1 left)
- High harmonics (POT 2 right)
- Modulate CV 2 with slow LFO for evolving tone

Result: Wide, evolving stereo bass
```

### Example 2: Through-Zero FM from LFO
```
Patch:
- Slow LFO (0.5Hz) → IN 1
- OUT 1 → Mixer
- Mode: Mono Dual

Settings:
- Medium frequency (POT 1 center)
- Low harmonics (POT 2 left)
- CV 2 around 3V for strong FM

Result: Smoothly evolving timbral shifts
```

### Example 3: Dual Voice Harmony
```
Patch:
- Keyboard CV → CV 1
- Fixed voltage (2.5V) → CV 2
- OUT 1 → Left channel
- OUT 2 → Right channel
- Mode: Dual Independent

Settings:
- POT 1 for base pitch
- POT 2 for OSC2 harmonics
- CV 2 voltage sets interval (2.5V ≈ perfect fifth)

Result: Two-voice harmony with independent timbres
```

### Example 4: Ring Mod with External
```
Patch:
- External oscillator → IN 2
- OUT 1 → Mixer (dry)
- OUT 2 → Effects chain (wet)
- Mode: Main+Ring

Settings:
- Tune oscillators to taste
- Adjust harmonics for different ring mod character

Result: Classic ring modulation with DSF complexity
```

### Example 5: Audio-Rate FM from OSC
```
Patch:
- Fast oscillator (100-2kHz) → IN 1
- CV 2 with envelope or LFO
- OUT 1 → Output
- Mode: Mono Dual

Settings:
- POT 1 for carrier freq
- CV 2 dynamically controls FM amount
- POT 2 adjusts harmonic content

Result: Dynamic FM synthesis with envelope control
```

### Example 6: Sub Bass + Lead
```
Patch:
- Sequencer → CV 1
- OUT 1 → Bass amp (sub-octave)
- OUT 2 → Lead channel (main)
- Mode: Main+Sub
  
Wait! I mixed that up. Let's fix:

- OUT 1 → Lead channel (main)
- OUT 2 → Bass amp (sub-octave)
- Mode: Main+Sub

Settings:
- POT 1 for pitch tracking
- POT 2 for lead harmonics
- Alpha for timbral variation

Result: Automatic bass + lead layering
```

## Technical Notes

### Through-Zero FM Implementation

The through-zero implementation uses phase reversal:

1. Audio input modulates frequency
2. When frequency goes negative, phase direction reverses
3. Output signal inverts on each zero-crossing while negative
4. Creates smooth transition through zero

This is different from simple phase modulation and creates unique sidebands.

### Output Mode Switching

Mode switching is instant (no clicks) because:
- Phase continuity is maintained
- Second oscillator is always running
- Only output routing changes

### CPU Usage

Approximate CPU usage by mode:
- Mono Dual: ~15%
- Stereo Detune: ~25%
- Dual Independent: ~25%
- Main+Sub: ~25%
- Main+Ring: ~20% (without input), ~25% (with input)
- Main+Process: ~20% (without input), ~25% (with input)

Plenty of headroom for future features!

### Sample Rate

Default: 48kHz
- High enough for audio quality
- Low enough for efficient DSF computation
- Allows up to ~20kHz harmonics without aliasing

## Troubleshooting

**No sound from OUT 2:**
- Check output mode (encoder press to cycle)
- Verify you're in correct mode for desired effect
- Check cables/connections

**Through-zero FM not working:**
- Ensure audio is patched to IN 1 (not IN 2)
- Increase CV 2 voltage
- Try higher amplitude modulator source
- Check display shows "FM:X.XX"

**Ring mod sounds weak:**
- Ensure audio patched to IN 2
- Try Main+Ring mode
- Increase input signal level
- Check that both signals are present

**Stereo width not audible:**
- Check in Stereo Detune mode
- Listen in actual stereo (not summed mono)
- Detune is subtle by design

**Display shows wrong mode:**
- Encoder press to cycle modes
- Long-press encoder to see extended view
- Check mode indicator letter

## Advanced Techniques

### Cross-Patching

Use OUT 2 back into IN 1 for feedback FM:
```
OUT 1 → Mixer (clean output)
OUT 2 → IN 1 (feedback)
Mode: Stereo Detune
```
Creates self-modulating chaos! Start with low CV 2 and increase carefully.

### Parallel Processing

Split out both outputs to separate effects:
```
OUT 1 → Reverb → Left
OUT 2 → Delay → Right
Mode: Stereo Detune
```

### External Processing Chain

Use DSF as dynamic sidechain:
```
Drums → IN 2
OUT 2 → VCA control (envelope follower)
Mode: Main+Process
```

### Quantized Through-Zero

Patch quantizer between LFO and IN 1 for stepped TZ-FM.

## Future Expansion Ideas

Potential features for future versions:
- MIDI control over output mode
- Saveable presets per mode
- Cross-modulation between oscillators
- Variable detune amount (currently fixed)
- Morphing between output modes
- Per-mode memory of settings

These could be added by forking the project!
