#include "uzi_dsp.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kTwoPi = 2.0f * 3.14159265358979323846f;
}

void UziDsp::Init(float sampleRate)
{
    sampleRate_ = sampleRate;
    distortionLeft_.Reset();
    distortionRight_.Reset();
    spectral_.Init(sampleRate_);
    lfoPhase_ = 0.0f;
}

void UziDsp::Process(daisy::AudioHandle::InputBuffer in,
                     daisy::AudioHandle::OutputBuffer out,
                     size_t size,
                     const UziRuntime &runtime)
{
    DistortionSettings settings;
    const float wave = std::clamp(runtime.wave, 0.0f, 1.0f);
    settings.depth = std::clamp(wave * 1.5f, 0.0f, 2.0f);
    settings.folds = 1 + static_cast<int>(wave * 4.0f);
    settings.overdrive = runtime.overdrive;

    float inPeakL = 0.0f;
    float inPeakR = 0.0f;
    float outPeakL = 0.0f;
    float outPeakR = 0.0f;

    const float dryMix = std::clamp(1.0f - runtime.mix, 0.0f, 1.0f);
    const float wetMix = std::clamp(runtime.mix, 0.0f, 1.0f);

    size_t hopSize = 256;
    switch (runtime.blockSize)
    {
    case 0:
        hopSize = 128;
        break;
    case 1:
        hopSize = 256;
        break;
    default:
        hopSize = 512;
        break;
    }

    const float lfoHz = 0.05f + runtime.lfoFreq * 5.0f;
    const float lfoInc = kTwoPi * lfoHz / sampleRate_;

    for (size_t i = 0; i < size; ++i)
    {
        const float dryL = in[0][i];
        const float dryR = in[1][i];

        const float distortedL = distortionLeft_.ProcessSample(dryL, settings, inPeakL, outPeakL);
        const float distortedR = distortionRight_.ProcessSample(dryR, settings, inPeakR, outPeakR);

        lfoPhase_ += lfoInc;
        if (lfoPhase_ > kTwoPi)
        {
            lfoPhase_ -= kTwoPi;
        }
        const float lfoValue = std::sin(lfoPhase_);

        float wetL = 0.0f;
        float wetR = 0.0f;
        spectral_.ProcessSample(distortedL, distortedR, runtime, lfoValue, hopSize, wetL, wetR);

        out[0][i] = dryL * dryMix + wetL * wetMix;
        out[1][i] = dryR * dryMix + wetR * wetMix;
    }

    distortionLeft_.UpdateMakeup(inPeakL, outPeakL);
    distortionRight_.UpdateMakeup(inPeakR, outPeakR);
}
