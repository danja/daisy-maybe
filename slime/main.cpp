#include <algorithm>
#include <cmath>

#include "daisy_seed.h"
#include "kxmx_bluemchen.h"

#include "display.h"
#include "encoder_handler.h"
#include "spectral_processor.h"

using namespace daisy;
using namespace kxmx;

namespace
{
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

constexpr float kMinTime = 0.25f;
constexpr float kMaxTime = 16.0f;

float MapExpo(float value, float minVal, float maxVal)
{
    value = std::clamp(value, 0.0f, 1.0f);
    return minVal * powf(maxVal / minVal, value);
}
} // namespace

Bluemchen hw;
SpectralChannel channel1;
SpectralChannel channel2;

EncoderState encoderState;
int menuPageIndex = 0;

SpectralProcess processMode = SpectralProcess::Smear;
float timeBase = 1.0f;
float timeRatio = 1.0f;
float vibe = 0.0f;
float mix = 1.0f;

float windowLut[1024];

bool heartbeatOn = false;
uint32_t lastHeartbeatMs = 0;

const char *processNames[] = {"SMR", "SFT", "CMB", "FRZ"};

void UpdateControls()
{
    hw.ProcessAnalogControls();
    hw.ProcessDigitalControls();

    const float pot1 = hw.GetKnobValue(Bluemchen::CTRL_1);
    const float pot2 = hw.GetKnobValue(Bluemchen::CTRL_2);
    const float cv1 = hw.GetKnobValue(Bluemchen::CTRL_3);
    const float cv2 = hw.GetKnobValue(Bluemchen::CTRL_4);

    timeBase = MapExpo(std::clamp(pot1 + cv1, 0.0f, 1.0f), kMinTime, kMaxTime);
    vibe = std::clamp(pot2 + cv2, 0.0f, 1.0f);

    const int inc = hw.encoder.Increment();
    if (inc != 0)
    {
        switch (menuPageIndex)
        {
        case 0:
        {
            int idx = static_cast<int>(processMode) + inc;
            const int count = static_cast<int>(SpectralProcess::Count);
            if (idx < 0)
                idx = count - 1;
            if (idx >= count)
                idx = 0;
            processMode = static_cast<SpectralProcess>(idx);
            break;
        }
        case 1:
            timeRatio = std::clamp(timeRatio + inc * 0.05f, 0.25f, 4.0f);
            break;
        case 2:
            mix = std::clamp(mix + inc * 0.02f, 0.0f, 1.0f);
            break;
        default:
            break;
        }
    }

    UpdateEncoder(hw, encoderState, 3, menuPageIndex);
}

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    const float time1 = std::clamp(timeBase, kMinTime, kMaxTime);
    const float time2 = std::clamp(timeBase * timeRatio, kMinTime, kMaxTime);
    const float wet = mix;
    const float dry = 1.0f - wet;

    for (size_t i = 0; i < size; ++i)
    {
        const float wet1 = channel1.ProcessSample(in[0][i], processMode, time1, vibe);
        const float wet2 = channel2.ProcessSample(in[1][i], processMode, time2, vibe);
        out[0][i] = dry * in[0][i] + wet * wet1;
        out[1][i] = dry * in[1][i] + wet * wet2;
    }
}

DisplayData BuildDisplay()
{
    DisplayData data;
    const float time1 = std::clamp(timeBase, kMinTime, kMaxTime);
    const float time2 = std::clamp(timeBase * timeRatio, kMinTime, kMaxTime);
    data.processLabel = processNames[static_cast<int>(processMode)];
    data.time1 = time1;
    data.time2 = time2;
    data.vibe = vibe;
    data.mix = mix;
    data.menuPage = menuPageIndex;
    data.heartbeatOn = heartbeatOn;
    return data;
}

int main(void)
{
    hw.Init();
    hw.StartAdc();

    for (size_t i = 0; i < 1024; ++i)
    {
        const float phase = static_cast<float>(i) / 1024.0f;
        windowLut[i] = 0.5f - 0.5f * cosf(2.0f * static_cast<float>(M_PI) * phase);
    }

    channel1.Init(hw.AudioSampleRate(), windowLut);
    channel2.Init(hw.AudioSampleRate(), windowLut);

    hw.StartAudio(AudioCallback);

    uint32_t lastDisplayUpdate = 0;
    while (1)
    {
        UpdateControls();

        const uint32_t now = System::GetNow();
        if (now - lastHeartbeatMs > 250)
        {
            heartbeatOn = !heartbeatOn;
            lastHeartbeatMs = now;
        }
        if (now - lastDisplayUpdate > 33)
        {
            RenderDisplay(hw, BuildDisplay());
            lastDisplayUpdate = now;
        }
    }
}
