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

constexpr float kMinTime = 0.125f;
constexpr float kMaxTime = 32.0f;
constexpr float kInputGain = 1.4f;
constexpr float kOutputGain = 0.9f;
constexpr float kWetTrim = 0.8f;
constexpr float kPeakDecay = 0.95f;
constexpr size_t kDryDelaySamples = SpectralChannel::kFftSize;

float MapExpo(float value, float minVal, float maxVal)
{
    value = std::clamp(value, 0.0f, 1.0f);
    return minVal * powf(maxVal / minVal, value);
}

float SoftClipInput(float sample)
{
    const float absSample = std::fabs(sample);
    return sample / (1.0f + absSample);
}

float SoftClip(float sample)
{
    return std::clamp(sample, -1.0f, 1.0f);
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
bool bypass = false;
float peak1 = 0.0f;
float peak2 = 0.0f;
float peakIn = 0.0f;
float peakOut = 0.0f;
float peakInClip = 0.0f;
float peakWet = 0.0f;
float cpuPercent = 0.0f;
float cpuMs = 0.0f;
float cpuBudgetMs = 0.0f;
float sampleRate = 48000.0f;
float dryDelayL[kDryDelaySamples]{};
float dryDelayR[kDryDelaySamples]{};
size_t dryDelayIndex = 0;

float windowLut[1024];

bool heartbeatOn = false;
uint32_t lastHeartbeatMs = 0;

const char *processNames[] = {"Thru", "Smear", "Shift", "Comb", "Freeze", "Gate", "Tilt", "Fold", "Phase"};

void UpdateControls()
{
    hw.ProcessDigitalControls();

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
            timeRatio = std::clamp(timeRatio + inc * 0.05f, 0.125f, 8.0f);
            break;
        case 2:
            mix = std::clamp(mix + inc * 0.02f, 0.0f, 1.0f);
            break;
        case 3:
            bypass = !bypass;
            break;
        default:
            break;
        }
    }

    UpdateEncoder(hw, encoderState, 7, menuPageIndex);
}

void UpdateAnalogControls()
{
    hw.ProcessAnalogControls();

    const float pot1 = hw.GetKnobValue(Bluemchen::CTRL_1);
    const float pot2 = hw.GetKnobValue(Bluemchen::CTRL_2);
    const float cv1 = hw.GetKnobValue(Bluemchen::CTRL_3);
    const float cv2 = hw.GetKnobValue(Bluemchen::CTRL_4);

    const float pot1Bipolar = (pot1 - 0.5f) * 2.0f;
    const float pot2Bipolar = (pot2 - 0.5f) * 2.0f;
    const float cv1Bipolar = (cv1 - 0.5f) * 2.0f;
    const float cv2Bipolar = (cv2 - 0.5f) * 2.0f;
    const float timeControl = std::clamp(0.5f + 0.5f * (pot1Bipolar + cv1Bipolar), 0.0f, 1.0f);
    const float vibeControl = std::clamp(0.5f + 0.5f * (pot2Bipolar + cv2Bipolar), 0.0f, 1.0f);
    timeBase = MapExpo(timeControl, kMinTime, kMaxTime);
    vibe = vibeControl;
}

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    const uint32_t callbackStart = System::GetNow();
    const float time1 = std::clamp(timeBase, kMinTime, kMaxTime);
    const float time2 = std::clamp(timeBase * timeRatio, kMinTime, kMaxTime);
    const float wetMix = mix;
    const float dryMix = 1.0f - mix;
    const bool dryOnly = (mix <= 0.001f);
    float localPeak1 = 0.0f;
    float localPeak2 = 0.0f;
    float localPeakIn = 0.0f;
    float localPeakOut = 0.0f;
    float localPeakInClip = 0.0f;
    float localPeakWet = 0.0f;

    for (size_t i = 0; i < size; ++i)
    {
        const float inRaw1 = in[0][i] * kInputGain;
        const float inRaw2 = in[1][i] * kInputGain;
        localPeakIn = std::max(localPeakIn, std::fabs(inRaw1));
        localPeakIn = std::max(localPeakIn, std::fabs(inRaw2));
        const float in1 = SoftClipInput(inRaw1);
        const float in2 = SoftClipInput(inRaw2);
        localPeakInClip = std::max(localPeakInClip, std::fabs(in1));
        localPeakInClip = std::max(localPeakInClip, std::fabs(in2));
        if (bypass || dryOnly)
        {
            const float sample1 = in1 * kOutputGain;
            const float sample2 = in2 * kOutputGain;
            localPeak1 = std::max(localPeak1, std::fabs(sample1));
            localPeak2 = std::max(localPeak2, std::fabs(sample2));
            localPeakOut = std::max(localPeakOut, std::fabs(sample1));
            localPeakOut = std::max(localPeakOut, std::fabs(sample2));
            out[0][i] = SoftClip(sample1);
            out[1][i] = SoftClip(sample2);
            continue;
        }
        const float dry1 = dryDelayL[dryDelayIndex];
        const float dry2 = dryDelayR[dryDelayIndex];
        dryDelayL[dryDelayIndex] = in1;
        dryDelayR[dryDelayIndex] = in2;
        dryDelayIndex = (dryDelayIndex + 1) % kDryDelaySamples;
        const float wet1Raw = (processMode == SpectralProcess::Thru)
                                  ? in1
                                  : channel1.ProcessSample(in1, processMode, time1, vibe);
        const float wet2Raw = (processMode == SpectralProcess::Thru)
                                  ? in2
                                  : channel2.ProcessSample(in2, processMode, time2, vibe);
        const float wet1 = std::clamp(wet1Raw, -1.0f, 1.0f) * kWetTrim;
        const float wet2 = std::clamp(wet2Raw, -1.0f, 1.0f) * kWetTrim;
        localPeakWet = std::max(localPeakWet, std::fabs(wet1));
        localPeakWet = std::max(localPeakWet, std::fabs(wet2));
        const float mix1 = (dryMix * dry1 + wetMix * wet1) * kOutputGain;
        const float mix2 = (dryMix * dry2 + wetMix * wet2) * kOutputGain;
        localPeak1 = std::max(localPeak1, std::fabs(mix1));
        localPeak2 = std::max(localPeak2, std::fabs(mix2));
        localPeakOut = std::max(localPeakOut, std::fabs(mix1));
        localPeakOut = std::max(localPeakOut, std::fabs(mix2));
        out[0][i] = SoftClip(mix1);
        out[1][i] = SoftClip(mix2);
    }

    peak1 = std::max(localPeak1, peak1 * kPeakDecay);
    peak2 = std::max(localPeak2, peak2 * kPeakDecay);
    peakIn = std::max(localPeakIn, peakIn * kPeakDecay);
    peakOut = std::max(localPeakOut, peakOut * kPeakDecay);
    peakInClip = std::max(localPeakInClip, peakInClip * kPeakDecay);
    peakWet = std::max(localPeakWet, peakWet * kPeakDecay);
    const uint32_t callbackEnd = System::GetNow();
    const float elapsedMs = static_cast<float>(callbackEnd - callbackStart);
    const float budgetMs = (static_cast<float>(size) * 1000.0f) / sampleRate;
    const float load = budgetMs > 0.0f ? (elapsedMs / budgetMs) * 100.0f : 0.0f;
    cpuPercent = std::max(load, cpuPercent * kPeakDecay);
    cpuMs = elapsedMs;
    cpuBudgetMs = budgetMs;
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
    data.bypass = bypass;
    data.peak1 = peak1;
    data.peak2 = peak2;
    data.peakIn = peakIn;
    data.peakOut = peakOut;
    data.peakInClip = peakInClip;
    data.peakWet = peakWet;
    data.cpuPercent = cpuPercent;
    data.cpuMs = cpuMs;
    data.cpuBudgetMs = cpuBudgetMs;
    return data;
}

int main(void)
{
    hw.Init();
    hw.StartAdc();
    sampleRate = hw.AudioSampleRate();

    for (size_t i = 0; i < 1024; ++i)
    {
        const float phase = static_cast<float>(i) / 1024.0f;
        const float hann = 0.5f - 0.5f * cosf(2.0f * static_cast<float>(M_PI) * phase);
        windowLut[i] = sqrtf(std::max(hann, 0.0f));
    }

    channel1.Init(sampleRate, windowLut);
    channel2.Init(sampleRate, windowLut);

    hw.StartAudio(AudioCallback);

    uint32_t lastDisplayUpdate = 0;
    while (1)
    {
        UpdateControls();
        UpdateAnalogControls();

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
