# kxmx_bluemchen Firmware Collection

A set of firmwares for the [kxmx_bluemchen](https://kxmx-bluemchen.recursinging.com/) Eurorack module, including DSF synthesis, dual resonators, spectral processors (Slime, Uzi), and the Neurotic neural‑inspired suite.

See also [Flues synth experiments](https://github.com/danja/flues) and [disyn-esp32](https://github.com/danja/disyn-esp32)

## Projects

- **daisy-dsf**  *(not great)*: Discrete Summation Formula (DSF) synthesizer porting algorithms from the disyn browser synth project.
- **resonators** *(awesome)*: Dual delay‑line resonators with 1V/oct tracking, wavefolding, cross‑feedback, damping, calibration, and bipolar knob/CV offsets. 
- **slime** *(poor)*: Stereo FFT spectral processor with time smear, multiple spectral effects, debug pages, and bipolar knob/CV control. 
- **uzi**: *(better than slime, but still not great)* Stereo spectral phaser with distortion front-end, spectral notching, crossover, and LFO modulation.
- **neurotic**: *(good, but odd. Still tweaking)* 10‑algorithm neural‑inspired processor (says AI - there's nothing neural about it, it's more a mix of the above processing styles) with menu‑selectable models.

**Status 2026-01-15 :** all tested on hardware and work according to some measure. There are quite a few rough edges - menu order, various parameters could be tweaked etc. But **reonators** and **neurotic** are currently in my rack.

## daisy-dsf Features

*TODO update : the current version has drifted from this : the resonator and formant parts removed, a Trajectory algorithm added*

- **Six synthesis algorithms:**
  - Classic DSF (Moorer 1976) - Band-limited sawtooth-like waveforms
  - Modified FM - DSF with frequency modulation
  - Waveshape - DSF with soft-clipping distortion
  - Complex DSF - Multi-term summation formula
  - Resonator Delay - Dual independent resonator delays (1ms-250ms)
  - Formant Synth - Cascaded formant filter synthesis for vowel-like vocal timbres

- **Six output modes:**
  - Mono Dual - Same signal on both outputs
  - Stereo Detune - Detuned oscillators for width
  - Dual Independent - Two independent oscillators
  - Main+Sub - Sub-octave on output 2
  - Main+Ring - Ring modulation
  - Main+Process - External audio processing

- **MIDI support:**
  - Polyphonic on channels 1 & 2 (one note per channel)
  - Note pitch added to frequency controls
  - Velocity controls output gain
  - Works with existing pot/CV controls

- **Advanced features:**
  - Through-zero FM via audio input 1
  - External audio processing via input 2
  - Dual oscillators with independent control
  - V/Oct pitch tracking (5 octave range)
  - Real-time OLED display

- **Real-time control:**
  - Pot 1: Base frequency (55Hz - 7kHz)
  - Pot 2: Number of harmonics (1-50, context-dependent)
  - CV 1: V/Oct pitch control
  - CV 2: Alpha/rolloff parameter OR FM depth
  - Encoder: Algorithm selection (rotate), Output mode (press)
  - Audio In 1: Through-zero FM modulator
  - Audio In 2: External audio for processing/ring mod
  - MIDI In: Note control on channels 1 & 2

## Prerequisites

### Required Tools

1. **ARM GCC Toolchain**
   ```bash
   # macOS
   brew install gcc-arm-embedded
   
   # Ubuntu/Debian
   sudo apt-get install gcc-arm-none-eabi
   
   # Windows - download from ARM's website
   ```

2. **Make**
   ```bash
   # macOS
   brew install make
   
   # Ubuntu/Debian
   sudo apt-get install build-essential
   ```

3. **dfu-util** (for flashing)
   ```bash
   # macOS
   brew install dfu-util
   
   # Ubuntu/Debian
   sudo apt-get install dfu-util
   ```

### Required Libraries

Clone these repositories as siblings to this project:

```bash
# Navigate to your projects directory
cd ~/github  # or wherever you keep your projects

# Clone required libraries
git clone --recursive https://github.com/electro-smith/libDaisy.git
git clone --recursive https://github.com/electro-smith/DaisySP.git
git clone --recursive https://github.com/recursinging/kxmx_bluemchen.git

# Clone this project
git clone https://github.com/danja/daisy-maybe.git
```

Your directory structure should look like:
```
github/  (or your projects directory)
├── libDaisy/
├── DaisySP/
├── kxmx_bluemchen/
└── daisy-maybe/
    └── daisy-dsf/
        ├── main.cpp
        ├── dsf_oscillator.h
        ├── formant_synth.h
        ├── Makefile
        └── README.md
```

**Note**: The Makefile expects libDaisy, DaisySP, and kxmx_bluemchen to be two directories up from daisy-dsf/ (i.e., siblings of daisy-maybe/).

## Building

1. **Build libDaisy and DaisySP** (first time only):
   ```bash
   cd ~/github/libDaisy  # adjust path to your setup
   make

   cd ~/github/DaisySP
   make
   ```

2. **Build this project:**
   ```bash
   cd ~/github/daisy-maybe/daisy-dsf
   make
   ```

   Or use the build helper script:
   ```bash
   cd ~/github/daisy-maybe/daisy-dsf
   ./build.sh          # Build only
   ./build.sh clean    # Clean build artifacts
   ./build.sh flash    # Build and flash to hardware
   ./build.sh all      # Clean, build, and flash
   ```

   This will create `build/daisy-dsf.bin`

## Flashing to Hardware

### Method 1: DFU Mode (Recommended)

1. Connect your bluemchen via USB
2. Hold the **BOOT** button on the Daisy Seed
3. Press and release the **RESET** button
4. Release the **BOOT** button (Daisy is now in DFU mode)
5. Flash the firmware:
   ```bash
   make program-dfu
   ```

### Method 2: Web Programmer

1. Build the project: `make`
2. Go to https://electro-smith.github.io/Programmer/
3. Connect your Daisy Seed in DFU mode (see steps above)
4. Click "Choose File" and select `build/daisy-dsf.bin`
5. Click "Program"

## Debugging

For detailed debugging instructions with ST-Link, see [docs/debugging.md](docs/debugging.md).

**Quick start**: Connect your ST-Link to the Daisy Seed, then press **F5** in VS Code to start debugging.

## Usage

For detailed operation instructions, see [docs/dsf-manual.md](docs/dsf-manual.md).

Additional docs:
- [docs/resonators-manual.md](docs/resonators-manual.md)
- [docs/slime-manual.md](docs/slime-manual.md)
- [docs/uzi-manual.md](docs/uzi-manual.md)
- [docs/neurotic-manual.md](docs/neurotic-manual.md)
- [docs/dependencies.md](docs/dependencies.md)

### Quick Start

1. **Select an algorithm**: Rotate the encoder
2. **Adjust frequency**: Turn Knob 1
3. **Add harmonics**: Turn Knob 2
4. **Change output mode**: Short press the encoder
5. **View extended info**: Long press (>500ms) the encoder

### Controls

- **Knob 1**: Base frequency
  - Range: 55 Hz to 7040 Hz (7 octaves)
  - Maps exponentially for musical tuning
  - MIDI notes are added to this frequency

- **Knob 2**: Number of harmonics
  - Range: 1 to 50 partials
  - In Dual Independent mode: controls oscillator 2 harmonics
  - Lower values = purer tones, higher = richer timbres

- **CV Input 1**: V/Oct pitch control
  - Adds to base frequency
  - Range: 5 octaves
  - Works alongside MIDI input

- **CV Input 2**: Context-dependent
  - When > 0.1V: FM depth control
  - When near 0V: Alpha/rolloff parameter (0.0-0.99)
  - In Dual Independent mode: Oscillator 2 pitch offset

- **Audio Input 1**: Through-zero FM modulator
  - Modulates oscillator 1 frequency
  - Negative frequencies cause phase reversal
  - FM depth controlled by CV 2

- **Audio Input 2**: External audio
  - Ring modulation carrier (Main+Ring mode)
  - Audio to process (Main+Process mode)

- **MIDI Input**: Note control
  - Channel 1 → Oscillator 1 (Output 1)
  - Channel 2 → Oscillator 2 (Output 2)
  - Note pitch adds to frequency controls
  - Velocity scales output gain (0-127)
  - No MIDI = full gain (normal operation)

- **Encoder**:
  - Rotate: Cycle through algorithms
  - Short press: Cycle through output modes
  - Long press (>500ms): Toggle display view

### Output Modes

1. **Mono Dual** (M): Identical signal on both outputs
2. **Stereo Detune** (S): Output 2 detuned +0.5%
3. **Dual Independent** (D): Two independent oscillators
4. **Main+Sub** (B): Output 2 is sub-octave (-1 octave)
5. **Main+Ring** (R): Output 2 is ring modulated
6. **Main+Process** (P): Output 2 processes external audio

### Algorithms Explained

1. **Classic DSF**
   - Pure Moorer DSF formula
   - Sawtooth-like waveform with controlled spectral rolloff
   - Alpha parameter controls harmonic decay rate

2. **Modified FM**
   - Combines DSF with phase modulation
   - Alpha becomes FM modulation index
   - Creates metallic, bell-like timbres

3. **Waveshape**
   - Classic DSF followed by soft-clipping
   - Alpha controls distortion amount
   - Adds odd harmonics and warmth

4. **Complex DSF**
   - Multi-term DSF formula
   - Mixes fundamental with shifted harmonic series
   - Creates complex, evolving timbres

5. **Resonator Delay**
   - Dual independent resonator delays
   - Processes audio inputs through pitch-tracked delays
   - POT 1: Base delay time (1-250ms)
   - CV 1: V/Oct delay time modulation
   - CV 2: Delay ratio (1:1 to 1:4)
   - MIDI controls delay pitch
   - IN 1 → Delay 1 → OUT 1, IN 2 → Delay 2 → OUT 2

6. **Formant Synth**
   - Cascaded bandpass filter formant synthesis for vocal timbres
   - 5 vowel presets (A, E, I, O, U) with authentic formant frequencies
   - 2D vowel space control for expressive morphing
   - POT 1: F1 frequency (200-1000 Hz) - jaw/vowel height
   - POT 2: F2 frequency (500-3000 Hz) - tongue position
   - Encoder rotation: Cycle through vowel presets
   - Audio inputs used as excitation by default
   - MIDI enables internal sawtooth+noise excitation
   - Dual independent voices for stereo formant processing

## Troubleshooting

### Build Issues

**Error: "libDaisy not found"**
- Check that libDaisy is cloned in the parent directory
- Verify the path in Makefile matches your setup

**Error: "arm-none-eabi-gcc: command not found"**
- ARM toolchain not installed or not in PATH
- Install gcc-arm-embedded (see Prerequisites)

**Error: "bluemchen.h: No such file"**
- kxmx_bluemchen repo not cloned
- Check directory structure matches above

### Flashing Issues

**Error: "No DFU capable USB device found"**
- Daisy Seed not in DFU mode
- Try the BOOT+RESET sequence again
- Check USB cable (must support data, not just power)

**Error: "dfu-util: command not found"**
- Install dfu-util (see Prerequisites)

### Runtime Issues

**No sound output**
- Check Eurorack power connection
- Verify audio cable connections
- Try adjusting Knob 1 (frequency)

**Distorted/clipping output**
- This is normal for some algorithms at high harmonic counts
- Try reducing harmonics (Knob 2) or alpha (CV 2)

**Display not updating**
- Press RESET button on Daisy Seed
- Reflash firmware

**MIDI not working**
- Check MIDI cable is connected to bluemchen MIDI IN
- Verify sending on MIDI channels 1 or 2
- Try sending a note on event (velocity > 0)
- Check MIDI indicator on extended display (long press encoder)

## Algorithm Theory

### Discrete Summation Formula (DSF)

DSF synthesizes band-limited waveforms by expressing periodic functions as closed-form summations of sinusoids:

```
y(t) = [sin(ωt) - α·sin(ωt - Nωt)] / [1 + α² - 2α·cos(Nωt)]
```

Where:
- ω = fundamental frequency (radians/sec)
- N = number of harmonics
- α = rolloff parameter (0 < α < 1)

This formula generates N harmonics with amplitude rolloff controlled by α, without explicitly computing each harmonic (making it efficient).

### Advantages over other methods

- **Band-limited**: No aliasing artifacts
- **Efficient**: Closed-form computation
- **Controllable**: Simple parameters for complex spectra
- **Real-time**: Low CPU overhead

### References

- Moorer, J. A. (1976). "The synthesis of complex audio spectra by means of discrete summation formulas"
- Stilson & Smith (1996). "Alias-Free Digital Synthesis of Classic Analog Waveforms"
- Noise Engineering Loquelic Iteritas (commercial implementation)

## Development

### Debugging

See [docs/debugging.md](docs/debugging.md) for complete debugging instructions with ST-Link and OpenOCD.

The project includes VS Code debug configurations in `daisy-dsf/.vscode/` for:
- Debugging with ST-Link adapter
- Building and flashing from VS Code
- Peripheral register inspection (SVD file included)

### Modifying Algorithms

Edit `dsf_oscillator.h` to add new algorithms:

1. Add new enum value to `Algorithm`
2. Add case to `Process()` switch statement
3. Implement `ProcessYourAlgorithm()` method
4. Update `algorithmNames[]` in `main.cpp`

### Adding Controls

Edit `main.cpp` `UpdateControls()` function to map hardware to parameters.

### Optimizations

Current implementation prioritizes clarity. Possible optimizations:
- Cache trigonometric values
- Use lookup tables for waveshaping
- Implement SIMD operations
- Reduce floating-point operations

## License

This project inherits licenses from its dependencies:
- libDaisy: MIT License
- DaisySP: MIT License  
- kxmx_bluemchen: MIT License (software), CC-BY-NC-SA (hardware)

This code: MIT License

## Credits

- DSF algorithms inspired by the disyn browser synthesizer
- Hardware platform: kxmx_bluemchen by recursinging
- DSP platform: Electrosmith Daisy Seed
- DSP library: DaisySP

## Further Reading

- [Daisy Wiki](https://github.com/electro-smith/DaisyWiki/wiki)
- [kxmx_bluemchen Documentation](https://kxmx-bluemchen.recursinging.com/)
- [DSF Synthesis Theory](https://www.musicdsp.org/en/latest/Synthesis/68-discrete-summation-formula-dsf.html)
- [Original disyn project](https://github.com/danja/flues)
