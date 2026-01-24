#pragma once

#include "daisy_seed.h"
#include "neurotic_state.h"
#include "algos/neurotic_algos.h"

class NeuroticDsp
{
public:
    void Init(float sampleRate);
    void Process(daisy::AudioHandle::InputBuffer in,
                 daisy::AudioHandle::OutputBuffer out,
                 size_t size,
                 const NeuroticRuntime &runtime);

private:
    float sampleRate_ = 48000.0f;
    int currentAlgo_ = 0;
    float lfoPhase_ = 0.0f;
    NeuroticAlgoBank algos_{};
};
