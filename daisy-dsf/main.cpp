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
#include <cmath>
#include <cstdio>
#include "daisy_seed.h"
#include "daisysp.h"
#include "kxmx_bluemchen.h"
#include "util/PersistentStorage.h"
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
float pitchScale = 1.0f;
float pitchOffset = 0.0f;
float calibPhase = 0.0f;
float globalSampleRate = 48000.0f;
uint32_t lastCalibChangeMs = 0;
bool calibDirty = false;
int lastAlgorithm = 0;

struct CalibSettings
{
    float scale;
    float offset;

    bool operator!=(const CalibSettings &rhs) const
    {
        return scale != rhs.scale || offset != rhs.offset;
    }
};

CalibSettings savedCalib = {1.0f, 0.0f};
PersistentStorage<CalibSettings> *calibStorage = nullptr;

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
const int CALIBRATION_ALGORITHM = NUM_ALGORITHMS - 1;

enum EncoderPage
{
    PAGE_ALGO,
    PAGE_PARAM2,
    PAGE_PARAM3,
    PAGE_OUTPUT,
    PAGE_INPUT
};

EncoderPage encoderPage = PAGE_ALGO;

enum InputMode
{
    INPUT_REACTOR,
    INPUT_CROSSMOD,
    INPUT_EXCITER
};

InputMode inputMode = INPUT_REACTOR;
const int NUM_INPUT_MODES = 3;
const char *inputModeNames[] = {
    "Reactor",
    "CrossMod",
    "Exciter"};
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
    for (size_t i = 0; i < size; i++)
    {
        const float in1 = in[0][i];
        const float in2 = in[1][i];
        float sig1 = 0.0f;
        float sig2 = 0.0f;

        if (currentAlgorithm == CALIBRATION_ALGORITHM)
        {
            calibPhase = disyn::StepPhase(calibPhase, currentFreq, globalSampleRate);
            sig1 = std::sin(calibPhase * disyn::kTwoPi) * 0.5f;
            sig2 = sig1;
        }
        else
        {
            if (currentAlgorithm == static_cast<int>(disyn::AlgorithmType::TRAJECTORY))
            {
                const float modDepth = 0.35f;
                const float mod2 = std::clamp(currentParam2 + in1 * modDepth, 0.0f, 1.0f);
                const float mod3 = std::clamp(currentParam3 + in2 * modDepth, 0.0f, 1.0f);
                osc1.SetParam2(mod2);
                osc1.SetParam3(mod3);
            }
            else
            {
                osc1.SetParam2(currentParam2);
                osc1.SetParam3(currentParam3);
            }

            if (inputMode == INPUT_REACTOR && currentAlgorithm != static_cast<int>(disyn::AlgorithmType::TRAJECTORY))
            {
                const float modDepth = 0.4f;
                const float mod2 = std::clamp(currentParam2 + fabsf(in1) * modDepth, 0.0f, 1.0f);
                const float mod3 = std::clamp(currentParam3 + fabsf(in2) * modDepth, 0.0f, 1.0f);
                osc1.SetParam2(mod2);
                osc1.SetParam3(mod3);
            }
            else if (inputMode == INPUT_CROSSMOD)
            {
                const float fmDepth = 400.0f;
                osc1.SetFrequency(std::max(0.0f, currentFreq + in1 * fmDepth));
            }

            disyn::AlgorithmOutput primary = osc1.Process();
            sig1 = primary.primary;
            sig2 = primary.secondary;

            if (inputMode == INPUT_CROSSMOD)
            {
                const float mix = std::clamp(in2 * 0.5f + 0.5f, 0.0f, 1.0f);
                sig1 = sig1 * (1.0f - mix) + sig2 * mix;
            }
            else if (inputMode == INPUT_EXCITER && currentAlgorithm != static_cast<int>(disyn::AlgorithmType::TRAJECTORY))
            {
                sig1 += in1 * 0.4f;
                sig2 += in2 * 0.4f;
            }
        }

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

    float targetFreq = 0.0f;
    float param1 = 0.5f;

    if (currentAlgorithm == CALIBRATION_ALGORITHM)
    {
        pitchScale = 0.8f + pot1 * 0.4f;
        pitchOffset = (pot2 - 0.5f) * 2.0f; // +/- 1 octave
        const float base = 440.0f * powf(2.0f, pitchOffset);
        targetFreq = base * powf(2.0f, cv1 * 5.0f * pitchScale);
        param1 = 0.5f;
        param2 = 0.5f;
        param3 = 0.5f;

        const uint32_t now = System::GetNow();
        if (fabsf(pitchScale - savedCalib.scale) > 0.0005f || fabsf(pitchOffset - savedCalib.offset) > 0.005f)
        {
            calibDirty = true;
            lastCalibChangeMs = now;
        }

        if (calibDirty && (now - lastCalibChangeMs) > 1000 && calibStorage)
        {
            calibStorage->GetSettings().scale = pitchScale;
            calibStorage->GetSettings().offset = pitchOffset;
            calibStorage->Save();
            savedCalib = calibStorage->GetSettings();
            calibDirty = false;
        }
    }
    else
    {
        const float baseFreq = 55.0f * powf(2.0f, pot1 * 7.0f);
        const float cvMultiplier = powf(2.0f, cv1 * 5.0f * pitchScale);
        const float midiFreq = midiCh1.active ? MidiNoteToFrequency(midiCh1.note) : baseFreq;
        targetFreq = midiFreq * cvMultiplier * powf(2.0f, pitchOffset);
        param1 = std::clamp(pot2 + (cv2 - 0.5f), 0.0f, 1.0f);
    }

    const float smoothedFreq = freqSmooth.Process(targetFreq);
    currentFreq = smoothedFreq;

    osc1.SetFrequency(currentFreq);
    osc2.SetFrequency(currentFreq * 1.005f);

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
            if (currentAlgorithm != CALIBRATION_ALGORITHM)
            {
                osc1.SetAlgorithm(currentAlgorithm);
                osc2.SetAlgorithm(currentAlgorithm);
                osc1.Reset();
                osc2.Reset();
            }
            break;
        case PAGE_PARAM2:
            if (currentAlgorithm != CALIBRATION_ALGORITHM)
                param2 = std::clamp(param2 + encInc * 0.01f, 0.0f, 1.0f);
            break;
        case PAGE_PARAM3:
            if (currentAlgorithm != CALIBRATION_ALGORITHM)
                param3 = std::clamp(param3 + encInc * 0.01f, 0.0f, 1.0f);
            break;
        case PAGE_OUTPUT:
            outputMode = static_cast<OutputMode>((outputMode + encInc + NUM_OUTPUT_MODES) % NUM_OUTPUT_MODES);
            break;
        case PAGE_INPUT:
            inputMode = static_cast<InputMode>((inputMode + encInc + NUM_INPUT_MODES) % NUM_INPUT_MODES);
            break;
        }
    }

    if (lastAlgorithm != currentAlgorithm)
    {
        if (lastAlgorithm == CALIBRATION_ALGORITHM && calibDirty && calibStorage)
        {
            calibStorage->GetSettings().scale = pitchScale;
            calibStorage->GetSettings().offset = pitchOffset;
            calibStorage->Save();
            savedCalib = calibStorage->GetSettings();
            calibDirty = false;
        }
        lastAlgorithm = currentAlgorithm;
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
            encoderPage = static_cast<EncoderPage>((encoderPage + 1) % 5);
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
    const char *pageLabel = "ALG";
    switch (encoderPage)
    {
    case PAGE_PARAM2:
        pageLabel = "P2";
        break;
    case PAGE_PARAM3:
        pageLabel = "P3";
        break;
    case PAGE_OUTPUT:
        pageLabel = "OUT";
        break;
    case PAGE_INPUT:
        pageLabel = "IN";
        break;
    default:
        pageLabel = "ALG";
        break;
    }
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
    hw.display.SetCursor(86, 0);
    hw.display.WriteString(pageLabel, Font_6x8, true);
    hw.display.SetCursor(110, 0);
    hw.display.WriteString(modeBuf, Font_6x8, true);

    snprintf(buf, sizeof(buf), "F:%.0fHz", currentFreq);
    hw.display.SetCursor(0, 12);
    hw.display.WriteString(buf, Font_6x8, true);

    if (currentAlgorithm == CALIBRATION_ALGORITHM)
    {
        snprintf(buf, sizeof(buf), "Scale:%.3f", pitchScale);
        hw.display.SetCursor(0, 20);
        hw.display.WriteString(buf, Font_6x8, true);

        snprintf(buf, sizeof(buf), "Offset:%+.2foct", pitchOffset);
        hw.display.SetCursor(0, 28);
        hw.display.WriteString(buf, Font_6x8, true);
    }
    else
    {
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
        else if (encoderPage == PAGE_INPUT)
        {
            snprintf(buf, sizeof(buf), ">In:%s", inputModeNames[inputMode]);
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
    }

    hw.display.Update();
}

int main(void)
{
    // Initialize hardware
    hw.Init();
    hw.StartAdc();
    float sampleRate = hw.AudioSampleRate();
    globalSampleRate = sampleRate;

    CalibSettings defaults;
    defaults.scale = 1.0f;
    defaults.offset = 0.0f;
    static PersistentStorage<CalibSettings> storage(hw.seed.qspi);
    storage.Init(defaults);
    calibStorage = &storage;
    savedCalib = storage.GetSettings();
    pitchScale = savedCalib.scale;
    pitchOffset = savedCalib.offset;

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
