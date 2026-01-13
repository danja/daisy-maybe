#include <algorithm>
#include <cmath>
#include <cstdio>

#include "daisy_seed.h"
#include "daisysp.h"
#include "kxmx_bluemchen.h"
#include "util/PersistentStorage.h"

using namespace daisy;
using namespace daisysp;
using namespace kxmx;

namespace
{
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

constexpr float kMinFreq = 20.0f;
constexpr float kMaxFreq = 2000.0f;
constexpr float kMaxFeedback = 0.99f;
constexpr float kMaxCross = 0.95f;
constexpr float kCalibTone = 440.0f;
constexpr float kTwoPi = 2.0f * static_cast<float>(M_PI);
constexpr size_t kMaxDelaySamples = 48000;

struct CalibSettings
{
    float scale;
    float offset;

    bool operator!=(const CalibSettings &rhs) const
    {
        return scale != rhs.scale || offset != rhs.offset;
    }
};

template <size_t max_size>
class DelayBuffer
{
  public:
    void Init()
    {
        Reset();
    }

    void Reset()
    {
        for(size_t i = 0; i < max_size; ++i)
        {
            line_[i] = 0.0f;
        }
        write_ptr_ = 0;
    }

    void SetDelay(float delay)
    {
        delay_ = std::clamp(delay, 1.0f, static_cast<float>(max_size - 2));
    }

    float Read() const
    {
        const int32_t delay_integral = static_cast<int32_t>(delay_);
        const float   delay_fractional = delay_ - static_cast<float>(delay_integral);
        const float a = line_[(write_ptr_ + delay_integral) % max_size];
        const float b = line_[(write_ptr_ + delay_integral + 1) % max_size];
        return a + (b - a) * delay_fractional;
    }

    void Write(float sample)
    {
        line_[write_ptr_] = sample;
        write_ptr_ = (write_ptr_ - 1 + max_size) % max_size;
    }

    void AddAt(float delay, float sample)
    {
        const float clamped = std::clamp(delay, 0.0f, static_cast<float>(max_size - 2));
        const int32_t delay_integral = static_cast<int32_t>(clamped);
        const float delay_fractional = clamped - static_cast<float>(delay_integral);
        const size_t idx = (write_ptr_ + delay_integral) % max_size;
        const size_t idx2 = (idx + 1) % max_size;
        line_[idx] += sample * (1.0f - delay_fractional);
        line_[idx2] += sample * delay_fractional;
    }

  private:
    float  line_[max_size];
    size_t write_ptr_ = 0;
    float  delay_ = 1.0f;
};
} // namespace

Bluemchen hw;
DelayBuffer<kMaxDelaySamples> delay1;
DelayBuffer<kMaxDelaySamples> delay2;
OnePole fbFilter1;
OnePole fbFilter2;

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
uint32_t encoderPressTime = 0;

uint32_t lastCalibChangeMs = 0;
bool calibDirty = false;
CalibSettings savedCalib = {1.0f, 0.0f};
PersistentStorage<CalibSettings> *calibStorage = nullptr;

float calibPhase = 0.0f;
float sampleRate = 48000.0f;
bool heartbeatOn = false;
uint32_t lastHeartbeatMs = 0;
bool ledOn = false;
uint32_t lastLedMs = 0;

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

MenuPage menuPage = PAGE_FEEDBACK;
MenuPage lastMenuPage = PAGE_FEEDBACK;

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

float SoftClipSample(float x)
{
    const float absx = fabsf(x);
    return x / (1.0f + absx);
}

void UpdateFeedbackFilters()
{
    const float minCut = 200.0f;
    const float maxCut = 8000.0f;
    const float cutoff = minCut + (1.0f - damp) * (maxCut - minCut);
    fbFilter1.SetFrequency(cutoff);
    fbFilter2.SetFrequency(cutoff);
}

void UpdateControlsFast()
{
    hw.ProcessAllControls();

    const float pot1 = hw.GetKnobValue(Bluemchen::CTRL_1);
    const float pot2 = hw.GetKnobValue(Bluemchen::CTRL_2);
    const float cv1 = hw.GetKnobValue(Bluemchen::CTRL_3);
    const float cv2 = hw.GetKnobValue(Bluemchen::CTRL_4);

    if (menuPage == PAGE_CALIB)
    {
        pitchScale = 0.8f + pot1 * 0.4f;
        pitchOffset = (pot2 - 0.5f) * 2.0f; // +/- 1 octave

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
        switch (menuPage)
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
            UpdateFeedbackFilters();
            break;
        case PAGE_MIX:
            mix = std::clamp(mix + encInc * 0.02f, 0.0f, 1.0f);
            break;
        case PAGE_CALIB:
            break;
        }
    }

    if (hw.encoder.RisingEdge())
    {
        encoderPressTime = System::GetNow();
    }
    if (hw.encoder.FallingEdge())
    {
        const uint32_t pressDuration = System::GetNow() - encoderPressTime;
        if (pressDuration > 500)
        {
            encoderLongPress = !encoderLongPress;
        }
        else
        {
            menuPage = static_cast<MenuPage>((menuPage + 1) % kNumPages);
        }
    }

    if (lastMenuPage != menuPage)
    {
        lastMenuPage = menuPage;
    }
}

void HandleCalibrationSave()
{
    if (!calibDirty || !calibStorage)
    {
        return;
    }

    const uint32_t now = System::GetNow();
    if ((now - lastCalibChangeMs) > 1000 || lastMenuPage != menuPage)
    {
        calibStorage->GetSettings().scale = pitchScale;
        calibStorage->GetSettings().offset = pitchOffset;
        calibStorage->Save();
        savedCalib = calibStorage->GetSettings();
        calibDirty = false;
    }
}

void UpdateDisplay()
{
    hw.display.Fill(false);

    char buf[32];

    hw.display.SetCursor(0, 0);
    snprintf(buf, sizeof(buf), "Resonators");
    hw.display.WriteString(buf, Font_6x8, true);

    hw.display.SetCursor(90, 0);
    hw.display.WriteString(menuNames[menuPage], Font_6x8, true);
    hw.display.SetCursor(120, 0);
    hw.display.WriteChar(heartbeatOn ? '.' : ' ', Font_6x8, true);

    if (menuPage == PAGE_CALIB)
    {
        snprintf(buf, sizeof(buf), "Scale:%.3f", pitchScale);
        hw.display.SetCursor(0, 12);
        hw.display.WriteString(buf, Font_6x8, true);

        snprintf(buf, sizeof(buf), "Offset:%+.2fo", pitchOffset);
        hw.display.SetCursor(0, 20);
        hw.display.WriteString(buf, Font_6x8, true);

        snprintf(buf, sizeof(buf), "Tone:%.1f", kCalibTone);
        hw.display.SetCursor(0, 28);
        hw.display.WriteString(buf, Font_6x8, true);
    }
    else
    {
        snprintf(buf, sizeof(buf), "F1:%.1f F2:%.1f", currentFreq, currentFreq2);
        hw.display.SetCursor(0, 12);
        hw.display.WriteString(buf, Font_6x8, true);

        snprintf(buf, sizeof(buf), "FB:%.2f D:%.2f", feedback, damp);
        hw.display.SetCursor(0, 20);
        hw.display.WriteString(buf, Font_6x8, true);

        switch (menuPage)
        {
        case PAGE_FEEDBACK:
            snprintf(buf, sizeof(buf), ">FB:%.2f", feedback);
            break;
        case PAGE_CROSS12:
            snprintf(buf, sizeof(buf), ">X12:%.2f", cross12);
            break;
        case PAGE_CROSS21:
            snprintf(buf, sizeof(buf), ">X21:%.2f", cross21);
            break;
        case PAGE_INPUT_POS:
            snprintf(buf, sizeof(buf), ">InP:%.2f", inputPos);
            break;
        case PAGE_DAMP:
            snprintf(buf, sizeof(buf), ">Damp:%.2f", damp);
            break;
        case PAGE_MIX:
            snprintf(buf, sizeof(buf), ">Mix:%.2f", mix);
            break;
        default:
            snprintf(buf, sizeof(buf), ">FB:%.2f", feedback);
            break;
        }

        hw.display.SetCursor(0, 28);
        hw.display.WriteString(buf, Font_6x8, true);
    }

    if (encoderLongPress && menuPage != PAGE_CALIB)
    {
        snprintf(buf, sizeof(buf), "X12 %.2f X21 %.2f", cross12, cross21);
        hw.display.SetCursor(0, 28);
        hw.display.WriteString(buf, Font_6x8, true);
    }

    hw.display.Update();
}

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    UpdateControlsFast();

    const bool isCalib = (menuPage == PAGE_CALIB);
    const float localMix = mix;
    const float localFeedback = feedback;
    const float localCross12 = cross12;
    const float localCross21 = cross21;
    const float localInputPos = inputPos;
    const float dryMix = 1.0f - localMix;
    const float wetMix = localMix;

    const float delaySamples1 = sampleRate / std::max(currentFreq, 1.0f);
    const float delaySamples2 = sampleRate / std::max(currentFreq2, 1.0f);

    delay1.SetDelay(delaySamples1);
    delay2.SetDelay(delaySamples2);

    for (size_t i = 0; i < size; i++)
    {
        if (isCalib)
        {
            calibPhase = calibPhase + kCalibTone / sampleRate;
            if (calibPhase >= 1.0f)
                calibPhase -= 1.0f;
            const float tone = std::sin(calibPhase * kTwoPi) * 0.5f;
            out[0][i] = tone;
            out[1][i] = tone;
            continue;
        }

        const float in1 = SoftClipSample(in[0][i]);
        const float in2 = SoftClipSample(in[1][i]);

        const float y1 = delay1.Read();
        const float y2 = delay2.Read();

        float fb1 = y1 * localFeedback + y2 * localCross21;
        float fb2 = y2 * localFeedback + y1 * localCross12;

        fb1 = fbFilter1.Process(fb1);
        fb2 = fbFilter2.Process(fb2);

        float write1 = SoftClipSample(fb1);
        float write2 = SoftClipSample(fb2);

        if (localInputPos <= 0.001f)
        {
            write1 = SoftClipSample(write1 + in1);
            write2 = SoftClipSample(write2 + in2);
        }
        else
        {
            delay1.AddAt(delaySamples1 * localInputPos, in1);
            delay2.AddAt(delaySamples2 * localInputPos, in2);
        }

        delay1.Write(write1);
        delay2.Write(write2);

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

    delay1.Init();
    delay2.Init();

    fbFilter1.Init();
    fbFilter2.Init();
    UpdateFeedbackFilters();

    hw.display.Fill(false);
    hw.display.SetCursor(0, 0);
    hw.display.WriteString("Resonators", Font_6x8, true);
    hw.display.SetCursor(0, 12);
    hw.display.WriteString("Booting...", Font_6x8, true);
    hw.display.Update();

    CalibSettings defaults{1.0f, 0.0f};
    static PersistentStorage<CalibSettings> storage(hw.seed.qspi);
    storage.Init(defaults);
    calibStorage = &storage;
    savedCalib = storage.GetSettings();
    pitchScale = savedCalib.scale;
    pitchOffset = savedCalib.offset;

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
            UpdateDisplay();
            lastDisplayUpdate = now;
        }
    }
}
