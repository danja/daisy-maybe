# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

DSF (Discrete Summation Formula) synthesizer for the kxmx_bluemchen Eurorack module, built on the Electrosmith Daisy Seed platform. Implements band-limited waveform synthesis using various DSF algorithms from the disyn browser synth project, plus formant synthesis for vocal timbres ported from the Chatterbox project.

## Build System

### Prerequisites
The project requires external dependencies in the parent directory:
- `libDaisy/` - Core Daisy Seed library
- `DaisySP/` - DSP utilities library
- `kxmx_bluemchen/` - Hardware abstraction for the bluemchen module

These must be cloned with `--recursive` flag and built before building this project.

### Building

**Build the project:**
```bash
cd daisy-dsf
make
```

**Build using helper script:**
```bash
./build.sh          # Build only
./build.sh clean    # Clean build artifacts
./build.sh flash    # Build and flash to hardware
./build.sh all      # Clean, build, and flash
```

**One-time library setup (in parent directory):**
```bash
cd libDaisy && make && cd ..
cd DaisySP && make && cd ..
```

**Flash to hardware:**
```bash
# Put Daisy Seed in DFU mode: hold BOOT, press RESET, release BOOT
make program-dfu
```

The Makefile includes paths to the three external dependencies and uses libDaisy's core Makefile for ARM compilation.

## Architecture

### Core Components

**DSFOscillator (`daisy-dsf/dsf_oscillator.h`)**
- Header-only oscillator class implementing five DSF algorithms
- Pure C++ with no external dependencies beyond standard math
- Manages phase accumulation, frequency, harmonics, and alpha parameters
- Supports through-zero FM via phase reversal mechanism
- Includes resonator delay algorithm with dual independent delays

**FormantSynth (`daisy-dsf/formant_synth.h`)**
- Header-only formant synthesis class implementing cascaded bandpass filters
- Uses DaisySP Svf filters for 4-formant vocal tract modeling
- 5 vowel presets (A, E, I, O, U) with authentic formant frequencies
- Dual excitation modes: external audio (default) or internal sawtooth+noise (MIDI)
- 2D vowel space control via POT 1 (F1) and POT 2 (F2)

**Main Application (`daisy-dsf/main.cpp`)**
- Hardware initialization and control mapping
- Dual oscillator instances and dual formant synth instances for stereo/dual outputs
- Six output modes for different signal routing configurations
- Context-dependent controls (change function based on algorithm)
- Display rendering and encoder interface
- Real-time audio callback processing

### Signal Flow

```
Controls → UpdateControls() → DSFOscillator parameters
                            ↓
Audio Inputs → AudioCallback() → DSFOscillator::Process() → Audio Outputs
                            ↓
                     UpdateDisplay() → OLED screen
```

### Algorithm Implementation

The module implements six synthesis algorithms:

**Classic DSF:** Pure Moorer formula - sawtooth-like with controlled spectral rolloff
**Modified FM:** DSF with phase modulation for metallic timbres
**Waveshape:** Classic DSF followed by soft-clipping distortion
**Complex DSF:** Multi-term summation for evolving timbres
**Resonator Delay:** Dual independent delays (1-250ms) with pitch tracking for physical modeling
**Formant Synth:** 4-formant cascaded bandpass filters for vocal synthesis

The DSF algorithms compute N harmonics in closed form without explicit per-harmonic calculation, making them efficient for real-time synthesis. The formant synth uses acoustically-accurate formant frequencies for vowel sounds.

### Hardware Control Mapping

**Pots (context-dependent):**
- Pot 1: Base frequency (55Hz - 7040Hz, exponential) OR F1 formant (200-1000Hz) in Formant Synth mode
- Pot 2: Number of harmonics (1-50) OR secondary oscillator harmonics in dual mode OR F2 formant (500-3000Hz) in Formant Synth mode

**CV Inputs:**
- CV 1: V/Oct pitch control (5 octave range)
- CV 2: Alpha parameter OR FM depth (context dependent)

**Audio Inputs:**
- IN 1: Through-zero FM modulator OR formant excitation source
- IN 2: External audio for ring mod or processing OR formant excitation source

**Encoder (context-dependent):**
- Rotate: Cycle through algorithms OR vowel presets (in Formant Synth mode)
- Short press: Cycle through output modes
- Long press (>500ms): Toggle display view

**MIDI:**
- Channels 1 & 2: Note pitch (additive to pots/CV) and velocity (output gain)
- In Formant Synth mode: Enables internal sawtooth+noise excitation

### Output Modes

Six routing configurations controlled by encoder press:
1. **Mono Dual** - Identical signal both outputs
2. **Stereo Detune** - Second oscillator +0.5% detuned
3. **Dual Independent** - Two independent oscillators with separate control
4. **Main+Sub** - Sub-octave on output 2
5. **Main+Ring** - Ring modulation with external or internal carrier
6. **Main+Process** - External audio shaped by DSF envelope

### Through-Zero FM

Implemented via frequency sign detection:
- Negative frequencies trigger phase reversal flag
- Phase inversion applied to output when reversed
- Creates unique timbres impossible with standard FM

## Code Patterns

### Adding New Algorithms

**For DSF-based algorithms:**
1. Add enum to `DSFOscillator::Algorithm` in `dsf_oscillator.h`
2. Add case to `Process()` switch in `dsf_oscillator.h`
3. Implement `ProcessYourAlgorithm()` private method following existing pattern
4. Add algorithm name to `algorithmNames[]` array in `main.cpp`
5. Update `NUM_ALGORITHMS` constant in `main.cpp`

**For standalone algorithms (like Formant Synth):**
1. Create header-only class following `formant_synth.h` pattern
2. Include in `main.cpp` and instantiate global objects
3. Add algorithm name to `algorithmNames[]` and update `NUM_ALGORITHMS`
4. Add algorithm-specific handling in `AudioCallback()`, `UpdateControls()`, and `HandleMidiMessage()`
5. Add display rendering in `UpdateDisplay()`

### Control Smoothing

Use DaisySP `OnePole` filters for parameter smoothing to avoid zipper noise:
```cpp
OnePole smoother;
smoother.Init();
smoother.SetFrequency(10.0f);  // Hz cutoff
float smoothed = smoother.Process(rawValue);
```

### Audio Callback Constraints

- No memory allocation
- No system calls
- Keep processing minimal - runs at ~48kHz
- Use class members for state, not local statics

## Platform-Specific Notes

### Daisy Seed Platform
- ARM Cortex-M7 @ 480MHz
- 48kHz audio sample rate
- Hardware FPU available
- Use arm-none-eabi-gcc toolchain (GNU++17)

### Bluemchen Hardware
- 2x CV inputs (0-5V, 12-bit ADC)
- 2x pots (10k linear)
- 2x audio inputs
- 2x audio outputs
- Rotary encoder with switch
- 128x64 OLED display

### Display Rendering

Update at ~30Hz (not audio rate) to avoid performance impact:
```cpp
if (System::GetNow() - lastDisplayUpdate > 33) {
    UpdateDisplay();
    lastDisplayUpdate = System::GetNow();
}
```

## Mathematical Background

DSF synthesis generates N harmonics with amplitude rolloff controlled by alpha (0 < α < 1):

```
y(t) = [sin(ωt) - α·sin(ωt - Nωt)] / [1 + α² - 2α·cos(Nωt)]
```

Key advantages:
- Band-limited (no aliasing)
- Efficient closed-form computation
- Real-time performance
- Simple parametric control of complex spectra

Avoid division by zero: check denominator before computing (threshold 1e-10f).

## Development Workflow

When modifying parameters or algorithms:
1. Edit source files
2. Run `make` to compile
3. Put hardware in DFU mode
4. Run `make program-dfu` or `./build.sh flash`
5. Test on actual hardware (simulator not available)

The build outputs to `daisy-dsf/build/` directory with `.bin`, `.elf`, and `.map` files.
