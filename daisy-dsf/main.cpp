/**
 * DSF Oscillator for kxmx_bluemchen
 *
 * Implements Discrete Summation Formula oscillators
 * based on the disyn project algorithms
 *
 * Controls:
 * - Pot 1: Frequency (55Hz - 7kHz)
 * - Pot 2: Algorithm Param 1
 * - Encoder: Menu selection (Algorithm / Param 2 / Param 3 / Output)
 * - CV 1: V/Oct pitch control
 * - CV 2: Param 1 modulation
 *
 * Audio Inputs:
 * - IN 1/2: Unused in this firmware
 *
 * Audio Outputs:
 * - OUT 1: Main DSF oscillator
 * - OUT 2: Secondary (sub-osc, processed, or independent)
 */

#include <algorithm>
#include <cstdio>
#include "daisy_seed.h"
#include "daisysp.h"
#include "kxmx_bluemchen.h"
#include "disyn_algorithm_info.h"
#include "disyn_oscillator.h"

using namespace daisy;
using namespace daisysp;
using namespace kxmx;

Bluemchen hw;
disyn::DisynOscillator osc1, osc2;
OnePole freqSmooth;
OnePole param1Smooth;
OnePole param2Smooth;
OnePole param3Smooth;

float param2 = 0.5f;
float param3 = 0.5f;
float currentFreq = 440.0f;
float currentParam1 = 0.5f;
float currentParam2 = 0.5f;
float currentParam3 = 0.5f;

// Output modes
enum OutputMode
{
    OUTPUT_MONO,
    OUTPUT_STEREO,
    OUTPUT_DETUNE
};

OutputMode outputMode = OUTPUT_STEREO;
const int NUM_OUTPUT_MODES = 3;
const char *outputModeNames[] = {
    "Mono",
    "Stereo",
    "Detune"};

// Algorithm selection
int currentAlgorithm = 0;
const int NUM_ALGORITHMS = static_cast<int>(disyn::kAlgorithmCount);

enum EncoderPage
{
    PAGE_ALGO,
    PAGE_PARAM2,
    PAGE_PARAM3,
    PAGE_OUTPUT
};

EncoderPage encoderPage = PAGE_ALGO;
bool encoderLongPress = false;
uint32_t encoderPressTime = 0;

// MIDI state tracking
struct MidiNoteState
{
    uint8_t note;     // MIDI note number (0-127)
    uint8_t velocity; // MIDI velocity (0-127)
    bool active;      // Whether a note is currently playing
};

MidiNoteState midiCh1 = {0, 127, false}; // Channel 1
float gain1 = 1.0f;                      // Gain (velocity-controlled)

// Helper function to convert MIDI note to frequency
float MidiNoteToFrequency(uint8_t note)
{
    return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

// MIDI message handler
void HandleMidiMessage(MidiEvent m)
{
    switch (m.type)
    {
    case NoteOn:
    {
        NoteOnEvent noteOn = m.AsNoteOn();
        if (noteOn.channel == 0)
        { // MIDI Channel 1
            midiCh1.note = noteOn.note;
            midiCh1.velocity = noteOn.velocity;
            midiCh1.active = (noteOn.velocity > 0); // Velocity 0 = note off
            gain1 = noteOn.velocity / 127.0f;
        }
        break;
    }
    case NoteOff:
    {
        NoteOffEvent noteOff = m.AsNoteOff();
        if (noteOff.channel == 0)
        { // MIDI Channel 1
            midiCh1.active = false;
            gain1 = 1.0f;
        }
        break;
    }
    default:
        break;
    }
}

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    (void)in;
    for (size_t i = 0; i < size; i++)
    {
        disyn::AlgorithmOutput primary = osc1.Process();
        float sig1 = primary.primary;
        float sig2 = primary.secondary;

        if (outputMode == OUTPUT_MONO)
        {
            sig2 = sig1;
        }
        else if (outputMode == OUTPUT_DETUNE)
        {
            disyn::AlgorithmOutput detuned = osc2.Process();
            sig2 = detuned.primary;
        }

        out[0][i] = sig1 * gain1;
        out[1][i] = sig2 * gain1;
    }
}

void UpdateControls()
{
    hw.ProcessAllControls();

    const float pot1 = hw.GetKnobValue(Bluemchen::CTRL_1);
    const float pot2 = hw.GetKnobValue(Bluemchen::CTRL_2);
    const float cv1 = hw.GetKnobValue(Bluemchen::CTRL_3);
    const float cv2 = hw.GetKnobValue(Bluemchen::CTRL_4);

    const float baseFreq = 55.0f * powf(2.0f, pot1 * 7.0f);
    const float cvMultiplier = powf(2.0f, cv1 * 5.0f);
    const float midiFreq = midiCh1.active ? MidiNoteToFrequency(midiCh1.note) : baseFreq;
    const float smoothedFreq = freqSmooth.Process(midiFreq * cvMultiplier);
    currentFreq = smoothedFreq;

    osc1.SetFrequency(currentFreq);
    osc2.SetFrequency(currentFreq * 1.005f);

    float param1Raw = pot2 + (cv2 - 0.5f);
    float param1 = std::clamp(param1Raw, 0.0f, 1.0f);
    param1 = param1Smooth.Process(param1);
    currentParam1 = param1;

    osc1.SetParam1(currentParam1);
    osc2.SetParam1(currentParam1);

    int encInc = hw.encoder.Increment();
    if (encInc != 0)
    {
        switch (encoderPage)
        {
        case PAGE_ALGO:
            currentAlgorithm += encInc;
            if (currentAlgorithm < 0)
                currentAlgorithm = NUM_ALGORITHMS - 1;
            if (currentAlgorithm >= NUM_ALGORITHMS)
                currentAlgorithm = 0;
            osc1.SetAlgorithm(currentAlgorithm);
            osc2.SetAlgorithm(currentAlgorithm);
            osc1.Reset();
            osc2.Reset();
            break;
        case PAGE_PARAM2:
            param2 = std::clamp(param2 + encInc * 0.01f, 0.0f, 1.0f);
            break;
        case PAGE_PARAM3:
            param3 = std::clamp(param3 + encInc * 0.01f, 0.0f, 1.0f);
            break;
        case PAGE_OUTPUT:
            outputMode = static_cast<OutputMode>((outputMode + encInc + NUM_OUTPUT_MODES) % NUM_OUTPUT_MODES);
            break;
        }
    }

    currentParam2 = param2Smooth.Process(param2);
    currentParam3 = param3Smooth.Process(param3);

    osc1.SetParam2(currentParam2);
    osc1.SetParam3(currentParam3);
    osc2.SetParam2(currentParam2);
    osc2.SetParam3(currentParam3);

    if (hw.encoder.RisingEdge())
    {
        encoderPressTime = System::GetNow();
        encoderLongPress = false;
    }

    if (hw.encoder.FallingEdge())
    {
        uint32_t pressDuration = System::GetNow() - encoderPressTime;

        if (pressDuration > 500)
        { // Long press (>500ms)
            encoderLongPress = !encoderLongPress;
        }
        else
        { // Short press
            encoderPage = static_cast<EncoderPage>((encoderPage + 1) % 4);
        }
    }
}

void UpdateDisplay()
{
    hw.display.Fill(false);
    const auto &info = disyn::GetAlgorithmInfo(currentAlgorithm);
    char buf[32];

    const char modeChar = (outputMode == OUTPUT_MONO) ? 'M' : (outputMode == OUTPUT_STEREO) ? 'S' : 'D';
    char modeBuf[2] = {modeChar, '\0'};
    hw.display.SetCursor(0, 0);
    if (encoderPage == PAGE_ALGO)
    {
        snprintf(buf, sizeof(buf), ">%s", info.name);
    }
    else
    {
        snprintf(buf, sizeof(buf), " %s", info.name);
    }
    hw.display.WriteString(buf, Font_6x8, true);
    hw.display.SetCursor(110, 0);
    hw.display.WriteString(modeBuf, Font_6x8, true);

    snprintf(buf, sizeof(buf), "F:%.0fHz", currentFreq);
    hw.display.SetCursor(0, 12);
    hw.display.WriteString(buf, Font_6x8, true);

    const float p1Value = disyn::MapNormalized(info.param1, currentParam1);
    if (info.param1.integer)
        snprintf(buf, sizeof(buf), "P1 %s:%d", info.param1.label, static_cast<int>(p1Value + 0.5f));
    else
        snprintf(buf, sizeof(buf), "P1 %s:%.2f", info.param1.label, p1Value);
    hw.display.SetCursor(0, 20);
    hw.display.WriteString(buf, Font_6x8, true);

    if (encoderPage == PAGE_OUTPUT || encoderLongPress)
    {
        snprintf(buf, sizeof(buf), ">Out:%s", outputModeNames[outputMode]);
    }
    else if (encoderPage == PAGE_PARAM3)
    {
        const float p3Value = disyn::MapNormalized(info.param3, currentParam3);
        if (info.param3.integer)
            snprintf(buf, sizeof(buf), ">P3 %s:%d", info.param3.label, static_cast<int>(p3Value + 0.5f));
        else
            snprintf(buf, sizeof(buf), ">P3 %s:%.2f", info.param3.label, p3Value);
    }
    else
    {
        const float p2Value = disyn::MapNormalized(info.param2, currentParam2);
        if (info.param2.integer)
            snprintf(buf, sizeof(buf), "%sP2 %s:%d",
                     encoderPage == PAGE_PARAM2 ? ">" : " ",
                     info.param2.label,
                     static_cast<int>(p2Value + 0.5f));
        else
            snprintf(buf, sizeof(buf), "%sP2 %s:%.2f",
                     encoderPage == PAGE_PARAM2 ? ">" : " ",
                     info.param2.label,
                     p2Value);
    }
    hw.display.SetCursor(0, 28);
    hw.display.WriteString(buf, Font_6x8, true);

    hw.display.Update();
}

int main(void)
{
    // Initialize hardware
    hw.Init();
    hw.StartAdc();
    float sampleRate = hw.AudioSampleRate();

    osc1.Init(sampleRate);
    osc1.SetAlgorithm(currentAlgorithm);

    osc2.Init(sampleRate);
    osc2.SetAlgorithm(currentAlgorithm);

    // Initialize smoothing filters
    freqSmooth.Init();
    freqSmooth.SetFrequency(10.0f); // 10Hz lowpass

    param1Smooth.Init();
    param1Smooth.SetFrequency(10.0f);

    param2Smooth.Init();
    param2Smooth.SetFrequency(10.0f);

    param3Smooth.Init();
    param3Smooth.SetFrequency(10.0f);

    // Start audio
    hw.StartAudio(AudioCallback);

    // Main loop
    uint32_t lastDisplayUpdate = 0;
    while (1)
    {
        UpdateControls();

        // Process MIDI events
        hw.midi.Listen();
        while (hw.midi.HasEvents())
        {
            HandleMidiMessage(hw.midi.PopEvent());
        }

        // Update display at ~30Hz
        uint32_t now = System::GetNow();
        if (now - lastDisplayUpdate > 33)
        {
            UpdateDisplay();
            lastDisplayUpdate = now;
        }
    }
}
