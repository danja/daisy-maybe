#include "neurotic_dsp.h"

#include <algorithm>

void NeuroticDsp::Init(float sampleRate)
{
    sampleRate_ = sampleRate;
    algos_.Init(sampleRate_);
    currentAlgo_ = 0;
    lfoPhase_ = 0.0f;
}

void NeuroticDsp::Process(daisy::AudioHandle::InputBuffer in,
                          daisy::AudioHandle::OutputBuffer out,
                          size_t size,
                          const NeuroticRuntime &runtime)
{
    if (runtime.algoIndex != currentAlgo_)
    {
        currentAlgo_ = std::clamp(runtime.algoIndex, 0, 10);
        algos_.Reset(currentAlgo_);
    }

    const float mix = std::clamp(runtime.mix, 0.0f, 1.0f);
    const float dryMix = 1.0f - mix;
    const float wetMix = mix;
    const float trim = std::clamp(runtime.outTrim, 0.0f, 2.0f);
    const float lfoHz = 0.05f + runtime.lfoRate * 6.0f;
    const float lfoInc = (2.0f * 3.14159265358979323846f) * lfoHz / sampleRate_;

    for (size_t i = 0; i < size; ++i)
    {
        const float inL = in[0][i];
        const float inR = in[1][i];

        lfoPhase_ += lfoInc;
        if (lfoPhase_ > 2.0f * 3.14159265358979323846f)
            lfoPhase_ -= 2.0f * 3.14159265358979323846f;

        NeuroticRuntime local = runtime;
        local.lfoValue = std::sin(lfoPhase_);

        float wetL = 0.0f;
        float wetR = 0.0f;
        algos_.Process(currentAlgo_, inL, inR, local, wetL, wetR);

        out[0][i] = (inL * dryMix + wetL * wetMix) * trim;
        out[1][i] = (inR * dryMix + wetR * wetMix) * trim;
    }
}
