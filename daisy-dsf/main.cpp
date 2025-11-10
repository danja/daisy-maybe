/**
 * DSF Oscillator for kxmx_bluemchen
 * 
 * Implements Discrete Summation Formula oscillators
 * based on the disyn project algorithms
 * 
 * Controls:
 * - Pot 1: Frequency (55Hz - 7kHz)
 * - Pot 2: Number of harmonics (1-50) OR Output mode
 * - Encoder: Algorithm selection / Mode toggle
 * - CV 1: V/Oct pitch control
 * - CV 2: Alpha/rolloff parameter OR FM depth
 * 
 * Audio Inputs:
 * - IN 1: Through-zero FM modulator
 * - IN 2: External audio for processing
 * 
 * Audio Outputs:
 * - OUT 1: Main DSF oscillator
 * - OUT 2: Secondary (sub-osc, processed, or independent)
 */

#include "daisy_seed.h"
#include "daisysp.h"
#include "kxmx_bluemchen.h"
#include "dsf_oscillator.h"

using namespace daisy;
using namespace daisysp;
using namespace kxmx;

Bluemchen hw;
DSFOscillator osc1, osc2;  // Dual oscillators
OnePole freqSmooth;
OnePole alphaSmooth;
OnePole fmDepthSmooth;

// Output modes
enum OutputMode {
    MONO_DUAL,        // Same signal on both outputs (mono)
    STEREO_DETUNE,    // Slightly detuned for stereo width
    DUAL_INDEPENDENT, // Two independent oscillators
    MAIN_SUB,         // Main + sub-octave
    MAIN_RING,        // Main + ring modulated
    MAIN_PROCESSED    // Main + through algorithm
};

OutputMode outputMode = STEREO_DETUNE;
const int NUM_OUTPUT_MODES = 6;
const char* outputModeNames[] = {
    "Mono Dual",
    "Stereo Detune",
    "Dual Indep",
    "Main+Sub",
    "Main+Ring",
    "Main+Process"
};

// Algorithm selection
int currentAlgorithm = 0;
const int NUM_ALGORITHMS = 5;
const char* algorithmNames[] = {
    "Classic DSF",
    "Modified FM",
    "Waveshape",
    "Complex DSF",
    "Resonator Delay"
};

// Through-zero FM parameters
float fmDepth = 0.0f;
bool encoderLongPress = false;
uint32_t encoderPressTime = 0;

// MIDI state tracking
struct MidiNoteState {
    uint8_t note;        // MIDI note number (0-127)
    uint8_t velocity;    // MIDI velocity (0-127)
    bool active;         // Whether a note is currently playing
};

MidiNoteState midiCh1 = {0, 127, false};  // Channel 1 -> Oscillator 1
MidiNoteState midiCh2 = {0, 127, false};  // Channel 2 -> Oscillator 2
float gain1 = 1.0f;  // Gain for oscillator 1 (velocity-controlled)
float gain2 = 1.0f;  // Gain for oscillator 2 (velocity-controlled)

// Helper function to convert MIDI note to frequency
float MidiNoteToFrequency(uint8_t note) {
    return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

// MIDI message handler
void HandleMidiMessage(MidiEvent m) {
    switch (m.type) {
        case NoteOn: {
            NoteOnEvent noteOn = m.AsNoteOn();
            // MIDI channels are 0-indexed in the API (0-15), but we want channels 1 & 2 (indices 0 & 1)
            if (noteOn.channel == 0) {  // MIDI Channel 1
                midiCh1.note = noteOn.note;
                midiCh1.velocity = noteOn.velocity;
                midiCh1.active = (noteOn.velocity > 0);  // Velocity 0 = note off
                // Update gain based on velocity (normalize to 0.0-1.0)
                gain1 = noteOn.velocity / 127.0f;
            } else if (noteOn.channel == 1) {  // MIDI Channel 2
                midiCh2.note = noteOn.note;
                midiCh2.velocity = noteOn.velocity;
                midiCh2.active = (noteOn.velocity > 0);
                gain2 = noteOn.velocity / 127.0f;
            }
            break;
        }
        case NoteOff: {
            NoteOffEvent noteOff = m.AsNoteOff();
            if (noteOff.channel == 0) {  // MIDI Channel 1
                midiCh1.active = false;
                gain1 = 1.0f;  // Return to full gain when note released
            } else if (noteOff.channel == 1) {  // MIDI Channel 2
                midiCh2.active = false;
                gain2 = 1.0f;
            }
            break;
        }
        default:
            break;
    }
}

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size) {
    for (size_t i = 0; i < size; i++) {
        // Read audio inputs
        float audioIn1 = in[0][i];  // Through-zero FM modulator OR Delay input 1
        float audioIn2 = in[1][i];  // External audio OR Delay input 2

        // Special handling for Resonator Delay algorithm
        if (currentAlgorithm == DSFOscillator::RESONATOR_DELAY) {
            // Pass audio inputs to oscillator
            osc1.SetAudioInput1(audioIn1);
            osc1.SetAudioInput2(audioIn2);

            // Process delays - osc1.Process() returns delay 1 output
            float sig1 = osc1.Process();
            // Get delay 2 output separately
            float sig2 = osc1.GetDelayOutput2();

            // Apply velocity-based gain (defaults to 1.0 when no MIDI input)
            out[0][i] = sig1 * gain1;
            out[1][i] = sig2 * gain2;
            continue;  // Skip normal DSF processing
        }

        // Normal DSF processing (non-delay algorithms)
        // Apply through-zero FM from audio input 1
        float modAmount = fmDepth * audioIn1 * 1000.0f; // Scale for frequency modulation
        float modulatedFreq = osc1.GetFreq() + modAmount;

        // Through-zero: allow negative frequencies (phase reversal)
        if (modulatedFreq < 0.0f) {
            osc1.SetThroughZero(true);
            osc1.SetFreq(-modulatedFreq);
        } else {
            osc1.SetThroughZero(false);
            osc1.SetFreq(modulatedFreq);
        }

        // Generate primary signal
        float sig1 = osc1.Process();
        float sig2 = 0.0f;
        
        // Generate secondary signal based on output mode
        switch(outputMode) {
            case MONO_DUAL:
                // Same signal on both outputs
                sig2 = sig1;
                break;
                
            case STEREO_DETUNE:
                // Slightly detuned second oscillator for stereo width
                sig2 = osc2.Process();
                break;
                
            case DUAL_INDEPENDENT:
                // Completely independent second oscillator
                sig2 = osc2.Process();
                break;
                
            case MAIN_SUB:
                // Sub-octave (half frequency)
                sig2 = osc2.Process();
                break;
                
            case MAIN_RING:
                // Ring modulation with external input or internal oscillator
                if (fabsf(audioIn2) > 0.01f) {
                    sig2 = sig1 * audioIn2;  // Ring mod with external
                } else {
                    sig2 = sig1 * osc2.Process();  // Internal ring mod
                }
                break;
                
            case MAIN_PROCESSED:
                // Process external audio through DSF algorithm
                if (fabsf(audioIn2) > 0.01f) {
                    // Use external audio, shaped by DSF characteristics
                    sig2 = audioIn2 * osc1.GetCurrentAmplitude();
                } else {
                    // No external input, use second oscillator
                    sig2 = osc2.Process();
                }
                break;
        }
        
        // Apply velocity-based gain (defaults to 1.0 when no MIDI input)
        out[0][i] = sig1 * gain1;
        out[1][i] = sig2 * gain2;
    }
}

void UpdateControls() {
    hw.ProcessAllControls();
    
    // Pot 1: Base frequency
    float pot1 = hw.GetKnobValue(Bluemchen::CTRL_1);
    float baseFreq = 55.0f * powf(2.0f, pot1 * 7.0f); // 55Hz to 7040Hz

    // CV 1: V/Oct pitch control (0-5V = 5 octaves)
    float cv1 = hw.GetKnobValue(Bluemchen::CTRL_3);
    float cvFreq = baseFreq * powf(2.0f, cv1 * 5.0f);

    // Add MIDI frequency offset for channel 1 if active
    float osc1Freq = cvFreq;
    if (midiCh1.active) {
        osc1Freq += MidiNoteToFrequency(midiCh1.note);
    }

    // Smooth frequency changes
    float smoothedFreq = freqSmooth.Process(osc1Freq);
    osc1.SetBaseFreq(smoothedFreq);  // Store base freq for TZ-FM
    
    // Set up second oscillator based on output mode
    switch(outputMode) {
        case STEREO_DETUNE: {
            float osc2Freq = smoothedFreq * 1.005f;  // Slight detune
            // Add MIDI offset for channel 2 if active
            if (midiCh2.active) {
                osc2Freq += MidiNoteToFrequency(midiCh2.note);
            }
            osc2.SetBaseFreq(osc2Freq);
            osc2.SetNumHarmonics(osc1.GetNumHarmonics());
            osc2.SetAlpha(osc1.GetAlpha());
            osc2.SetAlgorithm(osc1.GetCurrentAlgorithm());
            break;
        }

        case MAIN_SUB: {
            float osc2Freq = smoothedFreq * 0.5f;  // One octave down
            // Add MIDI offset for channel 2 if active
            if (midiCh2.active) {
                osc2Freq += MidiNoteToFrequency(midiCh2.note);
            }
            osc2.SetBaseFreq(osc2Freq);
            osc2.SetNumHarmonics(osc1.GetNumHarmonics());
            osc2.SetAlpha(osc1.GetAlpha());
            osc2.SetAlgorithm(osc1.GetCurrentAlgorithm());
            break;
        }

        case DUAL_INDEPENDENT: {
            // Second oscillator controlled independently (could use CV2)
            float cv2val = hw.GetKnobValue(Bluemchen::CTRL_4);
            float osc2Freq = smoothedFreq * powf(2.0f, cv2val * 2.0f);  // +2 octaves
            // Add MIDI offset for channel 2 if active
            if (midiCh2.active) {
                osc2Freq += MidiNoteToFrequency(midiCh2.note);
            }
            osc2.SetBaseFreq(osc2Freq);
            break;
        }

        case MAIN_RING: {
            // Ring mod carrier at different harmonic
            float osc2Freq = smoothedFreq * 1.5f;  // Perfect fifth
            // Add MIDI offset for channel 2 if active
            if (midiCh2.active) {
                osc2Freq += MidiNoteToFrequency(midiCh2.note);
            }
            osc2.SetBaseFreq(osc2Freq);
            break;
        }

        default: {
            float osc2Freq = smoothedFreq;
            // Add MIDI offset for channel 2 if active
            if (midiCh2.active) {
                osc2Freq += MidiNoteToFrequency(midiCh2.note);
            }
            osc2.SetBaseFreq(osc2Freq);
            break;
        }
    }
    
    // Pot 2: Changes function based on context
    float pot2 = hw.GetKnobValue(Bluemchen::CTRL_2);
    
    if (outputMode == DUAL_INDEPENDENT) {
        // In dual mode, pot 2 controls second oscillator's harmonics
        int harmonics2 = 1 + (int)(pot2 * 49.0f);
        osc2.SetNumHarmonics(harmonics2);
    } else {
        // Normally controls harmonics for both
        int harmonics = 1 + (int)(pot2 * 49.0f); // 1-50
        osc1.SetNumHarmonics(harmonics);
        osc2.SetNumHarmonics(harmonics);
    }
    
    // CV 2: FM Depth, Alpha, or Delay Ratio depending on algorithm
    float cv2 = hw.GetKnobValue(Bluemchen::CTRL_4);

    // Special handling for Resonator Delay algorithm
    if (currentAlgorithm == DSFOscillator::RESONATOR_DELAY) {
        // For Resonator Delay: Calculate delay times
        // POT 1: Base delay time (1ms - 250ms, exponential mapping)
        float baseDelayMs = 1.0f * powf(2.0f, pot1 * 7.97f); // 1ms to ~250ms

        // CV 1: V/Oct style offset (5 octaves range)
        float delayMultiplier = powf(2.0f, cv1 * 5.0f);
        float delay1Ms = baseDelayMs * delayMultiplier;

        // Add MIDI pitch offset (converts pitch to delay time offset)
        if (midiCh1.active) {
            // Higher MIDI note = shorter delay (higher pitch)
            float midiPitchRatio = MidiNoteToFrequency(midiCh1.note) / 440.0f;
            delay1Ms = delay1Ms / midiPitchRatio;  // Inverse relationship
        }

        // CV 2: Delay ratio from 1:1 to 1:4
        float delayRatio = 1.0f + (cv2 * 3.0f);  // 1.0 to 4.0
        float delay2Ms = delay1Ms * delayRatio;

        // Add MIDI pitch offset for channel 2
        if (midiCh2.active) {
            float midiPitchRatio2 = MidiNoteToFrequency(midiCh2.note) / 440.0f;
            delay2Ms = delay2Ms / midiPitchRatio2;
        }

        // Set delay times
        osc1.SetDelayTime1(delay1Ms);
        osc1.SetDelayTime2(delay2Ms);
        osc2.SetDelayTime1(delay1Ms);
        osc2.SetDelayTime2(delay2Ms);
    } else {
        // For other algorithms: FM Depth or Alpha
        // Check if we're receiving audio input for FM
        // (This is a simplified check - in practice you'd want to
        // detect actual audio presence)
        if (cv2 > 0.1f) {
            // Use CV2 as FM depth
            fmDepth = fmDepthSmooth.Process(cv2 * 2.0f);  // 0-2 range
        } else {
            // Use CV2 as alpha/rolloff
            float alpha = alphaSmooth.Process(cv2);
            osc1.SetAlpha(alpha);
            osc2.SetAlpha(alpha);
        }
    }

    // Encoder rotation: Algorithm selection
    int encInc = hw.encoder.Increment();
    if (encInc != 0) {
        currentAlgorithm += encInc;
        if (currentAlgorithm < 0) currentAlgorithm = NUM_ALGORITHMS - 1;
        if (currentAlgorithm >= NUM_ALGORITHMS) currentAlgorithm = 0;
        osc1.SetAlgorithm(static_cast<DSFOscillator::Algorithm>(currentAlgorithm));
        osc2.SetAlgorithm(static_cast<DSFOscillator::Algorithm>(currentAlgorithm));
    }
    
    // Encoder press: Short press = next output mode, Long press = toggle view
    if (hw.encoder.RisingEdge()) {
        encoderPressTime = System::GetNow();
        encoderLongPress = false;
    }
    
    if (hw.encoder.FallingEdge()) {
        uint32_t pressDuration = System::GetNow() - encoderPressTime;
        
        if (pressDuration > 500) {  // Long press (>500ms)
            encoderLongPress = !encoderLongPress;
        } else {  // Short press
            // Cycle through output modes
            outputMode = static_cast<OutputMode>((outputMode + 1) % NUM_OUTPUT_MODES);
        }
    }
}

void UpdateDisplay() {
    hw.display.Fill(false);
    
    if (encoderLongPress) {
        // Extended view: Show output mode and MIDI status
        hw.display.SetCursor(0, 0);
        hw.display.WriteString("OUTPUT MODE:", Font_6x8, true);

        hw.display.SetCursor(0, 12);
        hw.display.WriteString(outputModeNames[outputMode], Font_6x8, true);

        // Show MIDI status
        char buf[32];
        if (midiCh1.active || midiCh2.active) {
            if (midiCh1.active) {
                sprintf(buf, "M1:N%d V%d", midiCh1.note, midiCh1.velocity);
                hw.display.SetCursor(0, 20);
                hw.display.WriteString(buf, Font_6x8, true);
            }
            if (midiCh2.active) {
                sprintf(buf, "M2:N%d V%d", midiCh2.note, midiCh2.velocity);
                hw.display.SetCursor(0, 28);
                hw.display.WriteString(buf, Font_6x8, true);
            }
        } else if (fmDepth > 0.1f) {
            // Show FM depth if active and no MIDI
            sprintf(buf, "FM:%.2f", fmDepth);
            hw.display.SetCursor(0, 24);
            hw.display.WriteString(buf, Font_6x8, true);
        }

    } else {
        // Normal view: Algorithm and parameters
        
        // Algorithm name
        hw.display.SetCursor(0, 0);
        hw.display.WriteString(algorithmNames[currentAlgorithm], Font_6x8, true);
        
        // Frequency
        char buf[32];
        sprintf(buf, "F:%.1fHz", osc1.GetFreq());
        hw.display.SetCursor(0, 12);
        hw.display.WriteString(buf, Font_6x8, true);
        
        // Harmonics (show both if in dual independent mode)
        if (outputMode == DUAL_INDEPENDENT) {
            sprintf(buf, "H:%d/%d", osc1.GetNumHarmonics(), osc2.GetNumHarmonics());
        } else {
            sprintf(buf, "H:%d", osc1.GetNumHarmonics());
        }
        hw.display.SetCursor(0, 20);
        hw.display.WriteString(buf, Font_6x8, true);
        
        // Alpha or FM depth
        if (fmDepth > 0.1f) {
            sprintf(buf, "FM:%.2f", fmDepth);
        } else {
            sprintf(buf, "A:%.2f", osc1.GetAlpha());
        }
        hw.display.SetCursor(70, 20);
        hw.display.WriteString(buf, Font_6x8, true);
        
        // Output mode indicator (small)
        hw.display.SetCursor(110, 0);
        switch(outputMode) {
            case MONO_DUAL: hw.display.WriteString("M", Font_6x8, true); break;
            case STEREO_DETUNE: hw.display.WriteString("S", Font_6x8, true); break;
            case DUAL_INDEPENDENT: hw.display.WriteString("D", Font_6x8, true); break;
            case MAIN_SUB: hw.display.WriteString("B", Font_6x8, true); break;
            case MAIN_RING: hw.display.WriteString("R", Font_6x8, true); break;
            case MAIN_PROCESSED: hw.display.WriteString("P", Font_6x8, true); break;
        }
    }
    
    hw.display.Update();
}

int main(void) {
    // Initialize hardware
    hw.Init();
    float sampleRate = hw.AudioSampleRate();
    
    // Initialize both DSF oscillators
    osc1.Init(sampleRate);
    osc1.SetBaseFreq(440.0f);
    osc1.SetNumHarmonics(20);
    osc1.SetAlpha(0.5f);
    osc1.SetAlgorithm(DSFOscillator::CLASSIC_DSF);
    
    osc2.Init(sampleRate);
    osc2.SetBaseFreq(440.0f * 1.005f);  // Slight detune for default stereo
    osc2.SetNumHarmonics(20);
    osc2.SetAlpha(0.5f);
    osc2.SetAlgorithm(DSFOscillator::CLASSIC_DSF);
    
    // Initialize smoothing filters
    freqSmooth.Init();
    freqSmooth.SetFrequency(10.0f); // 10Hz lowpass
    
    alphaSmooth.Init();
    alphaSmooth.SetFrequency(5.0f); // 5Hz lowpass
    
    fmDepthSmooth.Init();
    fmDepthSmooth.SetFrequency(20.0f); // 20Hz lowpass for FM
    
    // Start audio
    hw.StartAudio(AudioCallback);
    
    // Main loop
    uint32_t lastDisplayUpdate = 0;
    while(1) {
        UpdateControls();

        // Process MIDI events
        hw.midi.Listen();
        while (hw.midi.HasEvents()) {
            HandleMidiMessage(hw.midi.PopEvent());
        }

        // Update display at ~30Hz
        uint32_t now = System::GetNow();
        if (now - lastDisplayUpdate > 33) {
            UpdateDisplay();
            lastDisplayUpdate = now;
        }
    }
}
