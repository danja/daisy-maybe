# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

DSF (Discrete Summation Formula) synthesizer for the kxmx_bluemchen Eurorack module, built on the Electrosmith Daisy Seed platform. Implements band-limited waveform synthesis using various DSF algorithms from the disyn browser synth project.

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
- Header-only oscillator class implementing four DSF algorithms
- Pure C++ with no external dependencies beyond standard math
- Manages phase accumulation, frequency, harmonics, and alpha parameters
- Supports through-zero FM via phase reversal mechanism

**Main Application (`daisy-dsf/main.cpp`)**
- Hardware initialization and control mapping
- Dual oscillator instances for stereo/dual outputs
- Six output modes for different signal routing configurations
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

### DSF Algorithm Implementation

All algorithms are based on the Discrete Summation Formula which generates band-limited waveforms:

**Classic DSF:** Pure Moorer formula - sawtooth-like with controlled spectral rolloff
**Modified FM:** DSF with phase modulation for metallic timbres
**Waveshape:** Classic DSF followed by soft-clipping distortion
**Complex DSF:** Multi-term summation for evolving timbres

The formula computes N harmonics in closed form without explicit per-harmonic calculation, making it efficient for real-time synthesis.

### Hardware Control Mapping

**Pots:**
- Pot 1: Base frequency (55Hz - 7040Hz, exponential)
- Pot 2: Number of harmonics (1-50) OR secondary oscillator harmonics in dual mode

**CV Inputs:**
- CV 1: V/Oct pitch control (5 octave range)
- CV 2: Alpha parameter OR FM depth (context dependent)

**Audio Inputs:**
- IN 1: Through-zero FM modulator
- IN 2: External audio for ring mod or processing

**Encoder:**
- Rotate: Cycle through algorithms
- Short press: Cycle through output modes
- Long press (>500ms): Toggle display view

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

### Adding New DSF Algorithms

1. Add enum to `DSFOscillator::Algorithm` in `dsf_oscillator.h:22-27`
2. Add case to `Process()` switch in `dsf_oscillator.h:52-65`
3. Implement `ProcessYourAlgorithm()` private method following existing pattern
4. Add algorithm name to `algorithmNames[]` array in `main.cpp:62-67`
5. Update `NUM_ALGORITHMS` constant in `main.cpp:61`

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
