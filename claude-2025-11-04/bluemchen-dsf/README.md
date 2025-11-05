# DSF Oscillator for kxmx_bluemchen

A Discrete Summation Formula (DSF) synthesizer for the kxmx_bluemchen Eurorack module, porting algorithms from the disyn browser synth project.

## Features

- **Four DSF algorithms:**
  - Classic DSF (Moorer 1976) - Band-limited sawtooth-like waveforms
  - Modified FM - DSF with frequency modulation
  - Waveshape - DSF with soft-clipping distortion
  - Complex DSF - Multi-term summation formula

- **Real-time control:**
  - Pot 1: Base frequency (55Hz - 7kHz)
  - Pot 2: Number of harmonics (1-50)
  - CV 1: V/Oct pitch control (5 octave range)
  - CV 2: Alpha/rolloff parameter (0.0-0.99)
  - Encoder: Algorithm selection

- **OLED display** shows current algorithm, frequency, harmonics, and alpha values

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

Clone these repositories in a parent directory:

```bash
# Create workspace
mkdir ~/bluemchen-workspace
cd ~/bluemchen-workspace

# Clone required libraries
git clone --recursive https://github.com/electro-smith/libDaisy.git
git clone --recursive https://github.com/electro-smith/DaisySP.git
git clone --recursive https://github.com/recursinging/kxmx_bluemchen.git

# Clone this project
git clone <this-repo-url> bluemchen-dsf
```

Your directory structure should look like:
```
bluemchen-workspace/
├── libDaisy/
├── DaisySP/
├── kxmx_bluemchen/
└── bluemchen-dsf/
    ├── main.cpp
    ├── dsf_oscillator.h
    ├── Makefile
    └── README.md
```

## Building

1. **Build libDaisy and DaisySP** (first time only):
   ```bash
   cd ~/bluemchen-workspace/libDaisy
   make
   
   cd ~/bluemchen-workspace/DaisySP
   make
   ```

2. **Build this project:**
   ```bash
   cd ~/bluemchen-workspace/bluemchen-dsf
   make
   ```

   This will create `build/bluemchen-dsf.bin`

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
4. Click "Choose File" and select `build/bluemchen-dsf.bin`
5. Click "Program"

## Usage

### Controls

- **Knob 1 (top)**: Base frequency
  - Range: 55 Hz to 7040 Hz (7 octaves)
  - Maps exponentially for musical tuning

- **Knob 2 (bottom)**: Number of harmonics
  - Range: 1 to 50 partials
  - Lower values = purer tones
  - Higher values = richer, more complex timbres

- **CV Input 1**: V/Oct pitch control
  - Adds to base frequency
  - Range: 5 octaves
  - Can be used with a keyboard or sequencer

- **CV Input 2**: Alpha parameter
  - Controls the rolloff/damping of harmonics
  - Range: 0.0 to 0.99
  - Lower = brighter, higher = darker

- **Encoder (rotate)**: Select algorithm
  - Rotate to cycle through 4 algorithms
  - Current algorithm displayed on OLED

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
