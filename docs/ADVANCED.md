# Advanced Customization Guide

## Adding New DSF Algorithms

### 1. Define the Algorithm

Add your new algorithm to `dsf_oscillator.h`:

```cpp
enum Algorithm {
    CLASSIC_DSF,
    MODIFIED_FM,
    WAVESHAPE,
    COMPLEX_DSF,
    MY_NEW_ALGORITHM  // Add here
};
```

### 2. Implement the Processing Function

In `dsf_oscillator.h`, add a private method:

```cpp
private:
    float ProcessMyNewAlgorithm() {
        // Your DSF formula here
        float N = (float)numHarmonics_;
        float a = alpha_;
        
        // Example: Triple-term DSF
        float term1 = /* ... */;
        float term2 = /* ... */;
        float term3 = /* ... */;
        
        return term1 + 0.5f * term2 + 0.25f * term3;
    }
```

### 3. Add to Switch Statement

In the `Process()` method:

```cpp
switch(algorithm_) {
    case CLASSIC_DSF:
        output = ProcessClassicDSF();
        break;
    // ... other cases ...
    case MY_NEW_ALGORITHM:
        output = ProcessMyNewAlgorithm();
        break;
}
```

### 4. Update Display Names

In `main.cpp`, update the names array:

```cpp
const char* algorithmNames[] = {
    "Classic DSF",
    "Modified FM",
    "Waveshape",
    "Complex DSF",
    "My Algorithm"  // Add here
};
```

## DSF Formula Variations

### Basic Sawtooth DSF
```cpp
float numerator = sinf(phase) - a * sinf(phase - N * phase);
float denominator = 1.0f + a*a - 2.0f*a*cosf(N * phase);
return numerator / denominator;
```

### Square Wave DSF
```cpp
// Only odd harmonics
float sum = 0.0f;
for (int k = 1; k <= N; k += 2) {
    sum += sinf(k * phase) / k;
}
return sum;
```

### Triangle DSF
```cpp
// Alternating odd harmonics
float sum = 0.0f;
int sign = 1;
for (int k = 1; k <= N; k += 2) {
    sum += sign * sinf(k * phase) / (k * k);
    sign = -sign;
}
return sum;
```

### Generalized DSF (Moorer)
```cpp
// With arbitrary harmonic spacing
float beta = 2.0f;  // harmonic multiplier
float numerator = sinf(phase) - a * sinf(beta * N * phase);
float denominator = 1.0f + a*a - 2.0f*a*cosf(beta * N * phase);
return numerator / denominator;
```

## Custom Parameters

### Adding a Third Knob Parameter

If you modify hardware or want to use existing controls differently:

```cpp
// In main.cpp, UpdateControls()
float pot2 = hw.GetKnobValue(Bluemchen::KNOB_2);

// Split into two parameters
float harmonics = pot2 * 2.0f;  // 0-2
if (harmonics < 1.0f) {
    // First half: control harmonics
    osc.SetNumHarmonics(1 + (int)(harmonics * 49.0f));
    osc.SetBeta(1.0f);  // default beta
} else {
    // Second half: control beta (harmonic spacing)
    osc.SetNumHarmonics(25);  // fixed
    osc.SetBeta(1.0f + (harmonics - 1.0f) * 3.0f);  // 1-4
}
```

### Using MIDI Input

The bluemchen has MIDI input! Use it for note control:

```cpp
// In main.cpp, add MIDI handling
MidiEvent event;
while (hw.midi.HasEvents()) {
    event = hw.midi.PopEvent();
    
    if (event.type == NoteOn) {
        float freq = mtof(event.data[0]);  // MIDI note to frequency
        osc.SetFreq(freq);
    }
}
```

## Optimization Techniques

### 1. Precompute Phase Increments

```cpp
class DSFOscillator {
private:
    float nPhaseInc_;  // N * phaseInc_ precomputed
    
    void UpdatePhaseIncrement() {
        phaseInc_ = (M_TWOPI * freq_) / sampleRate_;
        nPhaseInc_ = numHarmonics_ * phaseInc_;
    }
};
```

### 2. Use Lookup Tables for Trig

```cpp
#define LUT_SIZE 4096
static float sinLUT[LUT_SIZE];
static float cosLUT[LUT_SIZE];

// Initialize once
void InitLUT() {
    for (int i = 0; i < LUT_SIZE; i++) {
        float phase = (i * M_TWOPI) / LUT_SIZE;
        sinLUT[i] = sinf(phase);
        cosLUT[i] = cosf(phase);
    }
}

// Fast lookup
float FastSin(float phase) {
    int idx = (int)((phase / M_TWOPI) * LUT_SIZE) % LUT_SIZE;
    return sinLUT[idx];
}
```

### 3. SIMD Operations (Advanced)

The Cortex-M7 has hardware floating-point SIMD:

```cpp
#include "arm_math.h"

// Process 4 samples at once
float32_t input[4], output[4];
arm_sin_f32(input, output, 4);
```

## Stereo Processing

### Stereo Width from Single Oscillator

```cpp
void AudioCallback(...) {
    for (size_t i = 0; i < size; i++) {
        float sig = osc.Process();
        
        // Create pseudo-stereo with slight phase offset
        float leftPhase = osc.GetPhase();
        float rightPhase = leftPhase + 0.01f;  // slight detune
        
        out[0][i] = sig;
        out[1][i] = ProcessAtPhase(rightPhase);
    }
}
```

### Dual Oscillators

```cpp
DSFOscillator oscL, oscR;

void AudioCallback(...) {
    for (size_t i = 0; i < size; i++) {
        out[0][i] = oscL.Process();
        out[1][i] = oscR.Process();
    }
}
```

## Advanced DSF Techniques

### Formant Synthesis

Create vocal-like sounds by combining multiple DSF oscillators at formant frequencies:

```cpp
struct FormantDSF {
    DSFOscillator osc1, osc2, osc3;
    
    void SetVowel(char vowel) {
        switch(vowel) {
            case 'a':  // "ah"
                osc1.SetFreq(730);
                osc2.SetFreq(1090);
                osc3.SetFreq(2440);
                break;
            case 'e':  // "eh"
                osc1.SetFreq(530);
                osc2.SetFreq(1840);
                osc3.SetFreq(2480);
                break;
            // ... more vowels
        }
    }
    
    float Process() {
        return 0.5f * osc1.Process() +
               0.3f * osc2.Process() +
               0.2f * osc3.Process();
    }
};
```

### Dynamic Harmonic Control

Sweep harmonics over time for evolving timbres:

```cpp
class EvolvingDSF {
private:
    float harmonicPhase_;
    float harmonicRate_;
    
public:
    void Process() {
        // Modulate number of harmonics with LFO
        harmonicPhase_ += harmonicRate_;
        if (harmonicPhase_ >= M_TWOPI) harmonicPhase_ -= M_TWOPI;
        
        int harms = 10 + (int)(20.0f * (1.0f + sinf(harmonicPhase_)) / 2.0f);
        osc.SetNumHarmonics(harms);
        
        return osc.Process();
    }
};
```

### FM-DSF Hybrid

Combine frequency modulation with DSF:

```cpp
float ProcessFMDSF() {
    // FM modulator
    float modulator = sinf(phase_ * 3.0f);  // 3:1 ratio
    float modAmount = alpha_ * 2.0f;
    
    // Modulated DSF
    float modPhase = phase_ + modAmount * modulator;
    
    float N = (float)numHarmonics_;
    float a = alpha_;
    float num = sinf(modPhase) - a * sinf(modPhase - N * modPhase);
    float den = 1.0f + a*a - 2.0f*a*cosf(N * modPhase);
    
    return num / den;
}
```

## Debugging Tips

### 1. Add Debug Output to OLED

```cpp
// In main.cpp
hw.display.SetCursor(0, 28);
sprintf(buf, "DBG:%.3f", debugValue);
hw.display.WriteString(buf, Font_6x8, true);
```

### 2. Audio Rate Debugging

```cpp
// Output a test signal to check if audio is working
void AudioCallback(...) {
    static float testPhase = 0.0f;
    for (size_t i = 0; i < size; i++) {
        // 440Hz test tone
        out[0][i] = sinf(testPhase);
        out[1][i] = sinf(testPhase);
        testPhase += (M_TWOPI * 440.0f) / sampleRate;
        if (testPhase >= M_TWOPI) testPhase -= M_TWOPI;
    }
}
```

### 3. Scope Trigger

Use one audio output as a trigger signal:

```cpp
// Send trigger pulse on encoder press
if (hw.encoder.RisingEdge()) {
    triggerActive = true;
    triggerSamples = 0;
}

// In AudioCallback
if (triggerActive) {
    out[1][i] = (triggerSamples < 100) ? 1.0f : 0.0f;
    triggerSamples++;
    if (triggerSamples >= 1000) triggerActive = false;
}
```

## Resources

- [DaisySP Documentation](https://electro-smith.github.io/DaisySP/index.html)
- [libDaisy Documentation](https://electro-smith.github.io/libDaisy/index.html)
- [Daisy Forum](https://forum.electro-smith.com/)
- [Music DSP Archive](https://www.musicdsp.org/)
- [Julius O. Smith III - Digital Waveguide Modeling](https://ccrma.stanford.edu/~jos/)
