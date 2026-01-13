#include <algorithm>
#include <cmath>

#include "daisy_seed.h"
#include "kxmx_bluemchen.h"
#include "util/PersistentStorage.h"

#include "delay_lines.h"
#include "display.h"
#include "encoder_handler.h"
#include "filters.h"

using namespace daisy;
using namespace kxmx;

namespace
{
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

    constexpr float kMinFreq = 20.0f;
    constexpr float kMaxFreq = 2000.0f;
    constexpr float kMaxFeedback = 0.999f;
    constexpr float kMaxCross = 0.99f;
    constexpr float kCalibTone = 440.0f;
    constexpr float kTwoPi = 2.0f * static_cast<float>(M_PI);

    struct CalibSettings
    {
        float scale;
        float offset;

        bool operator!=(const CalibSettings &rhs) const
        {
            return scale != rhs.scale || offset != rhs.offset;
        }
    };
} // namespace

Bluemchen hw;
DelayLinePair delays;
FeedbackFilters feedbackFilters;
EncoderState encoderState;

float currentFreq = 440.0f;
float currentFreq2 = 440.0f;
float pitchScale = 1.0f;
float pitchOffset = 0.0f;
float feedback = 0.7f;
float cross12 = 0.0f;
float cross21 = 0.0f;
float inputPos = 0.0f;
float damp = 0.0f;
float mix = 1.0f;

bool encoderLongPress = false;

uint32_t lastCalibChangeMs = 0;
bool calibDirty = false;
bool calibSavePending = false;
CalibSettings savedCalib = {1.0f, 0.0f};
PersistentStorage<CalibSettings> *calibStorage = nullptr;

float calibPhase = 0.0f;
float sampleRate = 48000.0f;
bool heartbeatOn = false;
uint32_t lastHeartbeatMs = 0;
bool ledOn = false;
uint32_t lastLedMs = 0;
int lastEncInc = 0;
uint32_t lastEncIncMs = 0;
int encSteps = 0;
bool encPressed = false;
float encHeldMs = 0.0f;

enum MenuPage
{
    PAGE_CALIB,
    PAGE_FEEDBACK,
    PAGE_CROSS12,
    PAGE_CROSS21,
    PAGE_INPUT_POS,
    PAGE_DAMP,
    PAGE_MIX,
};

int menuPageIndex = PAGE_FEEDBACK;
int lastMenuPageIndex = PAGE_FEEDBACK;

const char *menuNames[] = {
    "CAL",
    "FB",
    "X12",
    "X21",
    "INP",
    "DAMP",
    "MIX",
};

constexpr int kNumPages = sizeof(menuNames) / sizeof(menuNames[0]);

float MapExpo(float value, float minVal, float maxVal)
{
    value = std::clamp(value, 0.0f, 1.0f);
    return minVal * powf(maxVal / minVal, value);
}

void UpdateControlsFast()
{
    hw.ProcessAnalogControls();
    hw.ProcessDigitalControls();

    const float pot1 = hw.GetKnobValue(Bluemchen::CTRL_1);
    const float pot2 = hw.GetKnobValue(Bluemchen::CTRL_2);
    const float cv1 = hw.GetKnobValue(Bluemchen::CTRL_3);
    const float cv2 = hw.GetKnobValue(Bluemchen::CTRL_4);

    if (menuPageIndex == PAGE_CALIB)
    {
        pitchScale = 0.8f + pot1 * 0.4f;
        pitchOffset = (pot2 - 0.5f) * 2.0f;

        const uint32_t now = System::GetNow();
        if (fabsf(pitchScale - savedCalib.scale) > 0.0005f || fabsf(pitchOffset - savedCalib.offset) > 0.005f)
        {
            calibDirty = true;
            lastCalibChangeMs = now;
        }
    }
    else
    {
        const float baseFreq = MapExpo(pot1, kMinFreq, kMaxFreq);
        const float cvMultiplier = powf(2.0f, cv1 * 5.0f * pitchScale);
        const float pitchMultiplier = powf(2.0f, pitchOffset);
        currentFreq = baseFreq * cvMultiplier * pitchMultiplier;

        const float offsetOct = (pot2 - 0.5f) * 2.0f + cv2 * 1.0f;
        const float offsetClamped = std::clamp(offsetOct, -2.0f, 2.0f);
        currentFreq2 = currentFreq * powf(2.0f, offsetClamped);
    }

    int encInc = hw.encoder.Increment();
    if (encInc != 0)
    {
        lastEncInc = encInc;
        lastEncIncMs = System::GetNow();
        encSteps += encInc;
    }
    encPressed = hw.encoder.Pressed();
    encHeldMs = hw.encoder.TimeHeldMs();
    if (encInc != 0)
    {
        switch (static_cast<MenuPage>(menuPageIndex))
        {
        case PAGE_FEEDBACK:
            feedback = std::clamp(feedback + encInc * 0.02f, 0.0f, kMaxFeedback);
            break;
        case PAGE_CROSS12:
            cross12 = std::clamp(cross12 + encInc * 0.02f, 0.0f, kMaxCross);
            break;
        case PAGE_CROSS21:
            cross21 = std::clamp(cross21 + encInc * 0.02f, 0.0f, kMaxCross);
            break;
        case PAGE_INPUT_POS:
            inputPos = std::clamp(inputPos + encInc * 0.02f, 0.0f, 1.0f);
            break;
        case PAGE_DAMP:
            damp = std::clamp(damp + encInc * 0.02f, 0.0f, 1.0f);
            feedbackFilters.SetDamp(damp);
            break;
        case PAGE_MIX:
            mix = std::clamp(mix + encInc * 0.02f, 0.0f, 1.0f);
            break;
        case PAGE_CALIB:
            break;
        }
    }

    const int prevPage = menuPageIndex;
    UpdateEncoder(hw, encoderState, kNumPages, menuPageIndex, encoderLongPress);
    if (prevPage != menuPageIndex)
    {
        if (prevPage == PAGE_CALIB)
        {
            calibSavePending = true;
        }
        lastMenuPageIndex = menuPageIndex;
    }
}

void HandleCalibrationSave()
{
    if (!calibStorage)
    {
        return;
    }

    const uint32_t now = System::GetNow();
    if (calibDirty && (now - lastCalibChangeMs) > 1000)
    {
        calibSavePending = true;
    }

    if (calibSavePending)
    {
        calibStorage->GetSettings().scale = pitchScale;
        calibStorage->GetSettings().offset = pitchOffset;
        calibStorage->Save();
        savedCalib = calibStorage->GetSettings();
        calibDirty = false;
        calibSavePending = false;
    }
}

DisplayData BuildDisplayData()
{
    DisplayData data;
    data.isCalib = (menuPageIndex == PAGE_CALIB);
    data.pitchScale = pitchScale;
    data.pitchOffset = pitchOffset;
    data.currentFreq = currentFreq;
    data.currentFreq2 = currentFreq2;
    data.feedback = feedback;
    data.damp = damp;
    data.cross12 = cross12;
    data.cross21 = cross21;
    data.inputPos = inputPos;
    data.mix = mix;
    data.menuLabel = menuNames[menuPageIndex];
    data.encoderLongPress = encoderLongPress;
    data.heartbeatOn = heartbeatOn;
    data.menuIndex = menuPageIndex;
    if (System::GetNow() - lastEncIncMs > 250)
    {
        data.encoderInc = 0;
    }
    else
    {
        data.encoderInc = lastEncInc;
    }
    data.encoderPressed = encPressed;
    data.encoderHeldMs = encHeldMs;
    data.encoderSteps = encSteps;
    return data;
}

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    UpdateControlsFast();

    const bool isCalib = (menuPageIndex == PAGE_CALIB);
    const float localMix = mix;
    const float localFeedback = feedback;
    const float localCross12 = cross12;
    const float localCross21 = cross21;
    const float localInputPos = inputPos;
    const float dryMix = 1.0f - localMix;
    const float wetMix = localMix;

    const float delaySamples1 = sampleRate / std::max(currentFreq, 1.0f);
    const float delaySamples2 = sampleRate / std::max(currentFreq2, 1.0f);

    delays.SetDelayTimes(delaySamples1, delaySamples2);

    for (size_t i = 0; i < size; i++)
    {
        if (isCalib)
        {
            calibPhase += kCalibTone / sampleRate;
            if (calibPhase >= 1.0f)
                calibPhase -= 1.0f;
            const float tone = std::sin(calibPhase * kTwoPi) * 0.5f;
            out[0][i] = tone;
            out[1][i] = tone;
            continue;
        }

        const float in1 = SoftClipSample(in[0][i]);
        const float in2 = SoftClipSample(in[1][i]);

        const float y1 = delays.Read1();
        const float y2 = delays.Read2();

        float fb1 = y1 * localFeedback + y2 * localCross21;
        float fb2 = y2 * localFeedback + y1 * localCross12;

        fb1 = feedbackFilters.Process1(fb1);
        fb2 = feedbackFilters.Process2(fb2);

        float write1 = SoftClipSample(fb1);
        float write2 = SoftClipSample(fb2);

        if (localInputPos <= 0.001f)
        {
            write1 = SoftClipSample(write1 + in1);
            write2 = SoftClipSample(write2 + in2);
        }
        else
        {
            delays.AddAt1(delaySamples1 * localInputPos, in1);
            delays.AddAt2(delaySamples2 * localInputPos, in2);
        }

        delays.Write1(write1);
        delays.Write2(write2);

        const float wet1 = y1;
        const float wet2 = y2;

        out[0][i] = dryMix * in1 + wetMix * wet1;
        out[1][i] = dryMix * in2 + wetMix * wet2;
    }
}

int main(void)
{
    hw.Init();
    hw.StartAdc();

    sampleRate = hw.AudioSampleRate();

    delays.Init();
    feedbackFilters.Init();
    feedbackFilters.SetDamp(damp);

    CalibSettings defaults{1.0f, 0.0f};
    static PersistentStorage<CalibSettings> storage(hw.seed.qspi);
    storage.Init(defaults);
    calibStorage = &storage;
    savedCalib = storage.GetSettings();
    pitchScale = savedCalib.scale;
    pitchOffset = savedCalib.offset;

    hw.display.Fill(false);
    hw.display.SetCursor(0, 0);
    hw.display.WriteString("Resonators", Font_6x8, true);
    hw.display.SetCursor(0, 12);
    hw.display.WriteString("Booting...", Font_6x8, true);
    hw.display.Update();

    hw.StartAudio(AudioCallback);

    uint32_t lastDisplayUpdate = 0;
    while (1)
    {
        HandleCalibrationSave();

        const uint32_t now = System::GetNow();
        if (now - lastHeartbeatMs > 250)
        {
            heartbeatOn = !heartbeatOn;
            lastHeartbeatMs = now;
        }
        if (now - lastLedMs > 250)
        {
            ledOn = !ledOn;
            hw.seed.SetLed(ledOn);
            lastLedMs = now;
        }
        if (now - lastDisplayUpdate > 33)
        {
            RenderDisplay(hw, BuildDisplayData());
            lastDisplayUpdate = now;
        }
    }
}
